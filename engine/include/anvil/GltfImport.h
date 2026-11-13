#pragma once
// ANVIL M117 - glTF 2.0 / GLB importer for skeletons, animation clips and skinned meshes (stage A2a).
// Header-only, no external dependencies (own JSON parser). Little-endian hosts only (all current targets).
// Supported: .gltf (external .bin / data: URIs) and .glb; accessors with strides + normalized integers;
// node TRS or matrix; skin 0 (joints may be in any order - bones are re-ordered so parents precede children);
// animation channels translation/rotation/scale with LINEAR, STEP and CUBICSPLINE interpolation.
// STEP is preserved by inserting a hold key just before the next key; CUBICSPLINE is baked to 8 samples per segment,
// so the runtime Animator (which interpolates linearly between keys) reproduces the authored curves.
// Not supported (reported in warnings): sparse accessors, morph target weights, more than one skin,
// non-joint nodes between two joints (bind pose is composed correctly, animated tracks may differ).
#include "anvil/AnimationRuntime.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace anvil {

struct SkinnedVertex {
    Vec3 position{}, normal{0,1,0};
    float u = 0, v = 0;
    std::uint16_t joints[4] = {0,0,0,0};   // indices into GltfImportResult::bones
    float weights[4] = {1,0,0,0};
};
struct SkinnedMeshData { std::string name; std::vector<SkinnedVertex> vertices; std::vector<std::uint32_t> indices; };
struct GltfImportResult {
    bool ok = false; std::string error; std::vector<std::string> warnings;
    std::vector<Bone> bones;                 // parent-before-child, ready for anvil::Skeleton
    std::vector<AnimMat4> inverseBind;       // per bone, same order as bones
    std::vector<AnimationClip> clips;        // tracks.size() == bones.size()
    std::vector<SkinnedMeshData> meshes;
};

// Decompose an affine matrix into translation / rotation / scale.
inline BoneTransform transformFromMatrix(const AnimMat4& m) {
    BoneTransform t; t.translation = {m.at(0,3), m.at(1,3), m.at(2,3)};
    float s[3];
    for (int c = 0; c < 3; ++c) s[c] = std::sqrt(m.at(0,c)*m.at(0,c) + m.at(1,c)*m.at(1,c) + m.at(2,c)*m.at(2,c));
    const float det = m.at(0,0)*(m.at(1,1)*m.at(2,2) - m.at(1,2)*m.at(2,1)) - m.at(0,1)*(m.at(1,0)*m.at(2,2) - m.at(1,2)*m.at(2,0)) + m.at(0,2)*(m.at(1,0)*m.at(2,1) - m.at(1,1)*m.at(2,0));
    if (det < 0) s[0] = -s[0];
    if (std::fabs(s[0]) < 1e-8f || std::fabs(s[1]) < 1e-8f || std::fabs(s[2]) < 1e-8f) return t;
    t.scale = {s[0], s[1], s[2]};
    float r[3][3];
    for (int row = 0; row < 3; ++row) for (int c = 0; c < 3; ++c) r[row][c] = m.at(row,c) / s[c];
    const float tr = r[0][0] + r[1][1] + r[2][2];
    Quaternion q;
    if (tr > 0) { const float k = std::sqrt(tr + 1) * 2; q = {(r[2][1]-r[1][2])/k, (r[0][2]-r[2][0])/k, (r[1][0]-r[0][1])/k, 0.25f*k}; }
    else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) { const float k = std::sqrt(1 + r[0][0]-r[1][1]-r[2][2]) * 2; q = {0.25f*k, (r[0][1]+r[1][0])/k, (r[0][2]+r[2][0])/k, (r[2][1]-r[1][2])/k}; }
    else if (r[1][1] > r[2][2]) { const float k = std::sqrt(1 + r[1][1]-r[0][0]-r[2][2]) * 2; q = {(r[0][1]+r[1][0])/k, 0.25f*k, (r[1][2]+r[2][1])/k, (r[0][2]-r[2][0])/k}; }
    else { const float k = std::sqrt(1 + r[2][2]-r[0][0]-r[1][1]) * 2; q = {(r[0][2]+r[2][0])/k, (r[1][2]+r[2][1])/k, 0.25f*k, (r[1][0]-r[0][1])/k}; }
    t.rotation = normalize(q);
    return t;
}

namespace gltf_detail {

// ------------------------------------------------------------------ JSON
struct Json {
    enum class T { Null, Bool, Num, Str, Arr, Obj } t = T::Null;
    bool b = false; double n = 0; std::string s; std::vector<Json> a; std::vector<std::pair<std::string, Json>> o;
    const Json* get(const std::string& k) const { if (t != T::Obj) return nullptr; for (auto& p : o) if (p.first == k) return &p.second; return nullptr; }
};
inline double jnum(const Json* j, double d = 0) { return j && j->t == Json::T::Num ? j->n : d; }
inline bool jbool(const Json* j, bool d = false) { return j && j->t == Json::T::Bool ? j->b : d; }
inline std::string jstr(const Json* j) { return j && j->t == Json::T::Str ? j->s : std::string(); }
inline const Json* jarr(const Json* j) { return j && j->t == Json::T::Arr ? j : nullptr; }

class JsonParser {
public:
    JsonParser(const char* b, const char* e) : p_(b), e_(e) {}
    bool parse(Json& out, std::string& err) {
        if (!value(out, 0)) { err = err_; return false; }
        ws(); if (p_ != e_) { err = "trailing characters after JSON"; return false; }
        return true;
    }
private:
    const char* p_; const char* e_; std::string err_;
    void ws() { while (p_ < e_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\r' || *p_ == '\t')) ++p_; }
    bool fail(const char* m) { err_ = m; return false; }
    bool lit(const char* w) { const std::size_t n = std::strlen(w); if (static_cast<std::size_t>(e_ - p_) >= n && std::strncmp(p_, w, n) == 0) { p_ += n; return true; } return false; }
    bool value(Json& o, int depth) {
        if (depth > 64) return fail("JSON nested too deeply");
        ws(); if (p_ >= e_) return fail("unexpected end of JSON");
        const char c = *p_;
        if (c == '{') {
            ++p_; o.t = Json::T::Obj; ws();
            if (p_ < e_ && *p_ == '}') { ++p_; return true; }
            for (;;) {
                ws(); std::string k;
                if (p_ >= e_ || *p_ != '"' || !str(k)) return fail("bad object key");
                ws(); if (p_ >= e_ || *p_ != ':') return fail("expected ':'"); ++p_;
                Json v; if (!value(v, depth + 1)) return false;
                o.o.emplace_back(std::move(k), std::move(v)); ws();
                if (p_ < e_ && *p_ == ',') { ++p_; continue; }
                if (p_ < e_ && *p_ == '}') { ++p_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++p_; o.t = Json::T::Arr; ws();
            if (p_ < e_ && *p_ == ']') { ++p_; return true; }
            for (;;) {
                Json v; if (!value(v, depth + 1)) return false;
                o.a.push_back(std::move(v)); ws();
                if (p_ < e_ && *p_ == ',') { ++p_; continue; }
                if (p_ < e_ && *p_ == ']') { ++p_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { o.t = Json::T::Str; return str(o.s); }
        if (lit("true")) { o.t = Json::T::Bool; o.b = true; return true; }
        if (lit("false")) { o.t = Json::T::Bool; o.b = false; return true; }
        if (lit("null")) { o.t = Json::T::Null; return true; }
        char buf[64]; std::size_t n = 0;
        while (p_ < e_ && n < 63 && ((*p_ >= '0' && *p_ <= '9') || *p_ == '-' || *p_ == '+' || *p_ == '.' || *p_ == 'e' || *p_ == 'E')) buf[n++] = *p_++;
        if (n == 0) return fail("unexpected character in JSON");
        buf[n] = 0; char* end = nullptr; o.n = std::strtod(buf, &end); o.t = Json::T::Num;
        return end == buf + n ? true : fail("bad number");
    }
    bool str(std::string& out) {
        ++p_;
        while (p_ < e_) {
            const char c = *p_++;
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (p_ >= e_) break;
            const char d = *p_++;
            switch (d) {
                case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                case 'u': {
                    if (e_ - p_ < 4) return fail("bad \\u escape");
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) { const char h = *p_++; cp <<= 4; cp |= (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0u; }
                    if (cp < 0x80) out += static_cast<char>(cp);
                    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                    else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                    break; }
                default: out += d;
            }
        }
        return fail("unterminated string");
    }
};

// ------------------------------------------------------------------ base64 / uri
inline bool base64Decode(std::string_view in, std::vector<std::uint8_t>& out) {
    out.clear(); unsigned acc = 0; int bits = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A'; else if (c >= 'a' && c <= 'z') v = c - 'a' + 26; else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62; else if (c == '/') v = 63; else if (c == '=') break; else if (c == ' ' || c == '\n' || c == '\r') continue; else return false;
        acc = (acc << 6) | static_cast<unsigned>(v); bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF)); }
    }
    return true;
}
inline bool safeRelativeUri(const std::string& u) {
    if (u.empty() || u[0] == '/' || u[0] == '\\' || u.find(':') != std::string::npos) return false;
    std::string seg;
    for (std::size_t i = 0; i <= u.size(); ++i) {
        if (i == u.size() || u[i] == '/' || u[i] == '\\') { if (seg == "..") return false; seg.clear(); } else seg += u[i];
    }
    return true;
}
inline std::string percentDecode(const std::string& s) {
    std::string r;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit(static_cast<unsigned char>(s[i+1])) && std::isxdigit(static_cast<unsigned char>(s[i+2]))) { r += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
        else r += s[i];
    }
    return r;
}

// ------------------------------------------------------------------ accessors
using Buffers = std::vector<std::vector<std::uint8_t>>;
struct Acc { std::vector<double> v; int comps = 0; std::size_t count = 0; };

inline bool readAccessor(const Json& root, const Buffers& bufs, int index, Acc& out, std::string& err) {
    const Json* accs = jarr(root.get("accessors"));
    if (!accs || index < 0 || index >= static_cast<int>(accs->a.size())) { err = "accessor index out of range"; return false; }
    const Json& a = accs->a[static_cast<std::size_t>(index)];
    const int ct = static_cast<int>(jnum(a.get("componentType")));
    int cs = 0;
    switch (ct) { case 5120: case 5121: cs = 1; break; case 5122: case 5123: cs = 2; break; case 5125: case 5126: cs = 4; break; default: err = "unsupported componentType"; return false; }
    const std::string type = jstr(a.get("type"));
    out.comps = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : type == "MAT2" ? 4 : type == "MAT3" ? 9 : type == "MAT4" ? 16 : 0;
    if (!out.comps) { err = "unsupported accessor type"; return false; }
    const double rawCount = jnum(a.get("count"));
    if (rawCount < 0 || rawCount > 67108864.0) { err = "accessor count out of range"; return false; }
    out.count = static_cast<std::size_t>(rawCount);
    out.v.assign(out.count * static_cast<std::size_t>(out.comps), 0.0);
    const Json* bvIdx = a.get("bufferView");
    if (!bvIdx || out.count == 0) return true;   // no bufferView -> zeros per spec
    const Json* bvs = jarr(root.get("bufferViews"));
    const int bi = static_cast<int>(jnum(bvIdx, -1));
    if (!bvs || bi < 0 || bi >= static_cast<int>(bvs->a.size())) { err = "bufferView index out of range"; return false; }
    const Json& bv = bvs->a[static_cast<std::size_t>(bi)];
    const int buf = static_cast<int>(jnum(bv.get("buffer"), -1));
    if (buf < 0 || buf >= static_cast<int>(bufs.size())) { err = "buffer index out of range"; return false; }
    const std::vector<std::uint8_t>& data = bufs[static_cast<std::size_t>(buf)];
    const std::size_t bvOff = static_cast<std::size_t>(jnum(bv.get("byteOffset"))), bvLen = static_cast<std::size_t>(jnum(bv.get("byteLength")));
    const std::size_t elem = static_cast<std::size_t>(out.comps * cs);
    const std::size_t stride = jnum(bv.get("byteStride")) > 0 ? static_cast<std::size_t>(jnum(bv.get("byteStride"))) : elem;
    const std::size_t start = bvOff + static_cast<std::size_t>(jnum(a.get("byteOffset")));
    const std::size_t last = start + (out.count - 1) * stride + elem;
    if (stride < elem || last > data.size() || last > bvOff + bvLen || last < start) { err = "accessor reads outside its buffer"; return false; }
    const bool norm = jbool(a.get("normalized"));
    for (std::size_t i = 0; i < out.count; ++i)
        for (int c = 0; c < out.comps; ++c) {
            const std::uint8_t* p = data.data() + start + i * stride + static_cast<std::size_t>(c * cs);
            double v = 0;
            switch (ct) {
                case 5120: { std::int8_t x; std::memcpy(&x, p, 1); v = norm ? std::max(x / 127.0, -1.0) : x; break; }
                case 5121: { std::uint8_t x; std::memcpy(&x, p, 1); v = norm ? x / 255.0 : x; break; }
                case 5122: { std::int16_t x; std::memcpy(&x, p, 2); v = norm ? std::max(x / 32767.0, -1.0) : x; break; }
                case 5123: { std::uint16_t x; std::memcpy(&x, p, 2); v = norm ? x / 65535.0 : x; break; }
                case 5125: { std::uint32_t x; std::memcpy(&x, p, 4); v = x; break; }
                default:   { float x; std::memcpy(&x, p, 4); v = x; break; }
            }
            out.v[i * static_cast<std::size_t>(out.comps) + static_cast<std::size_t>(c)] = v;
        }
    return true;
}

// ------------------------------------------------------------------ animation channels
struct Chan { int path = 0; int interp = 0; std::vector<float> times; std::vector<double> vals; int comps = 3; };  // path 0=T 1=R 2=S; interp 0=linear 1=step 2=cubic

inline void evalChan(const Chan& c, float t, float* out) {
    const int n = static_cast<int>(c.times.size()), cm = c.comps;
    const int stride = c.interp == 2 ? cm * 3 : cm, off = c.interp == 2 ? cm : 0;
    auto val = [&](int k, int j) { return static_cast<float>(c.vals[static_cast<std::size_t>(k) * static_cast<std::size_t>(stride) + static_cast<std::size_t>(off + j)]); };
    if (n == 1 || t <= c.times.front()) { for (int j = 0; j < cm; ++j) out[j] = val(0, j); return; }
    if (t >= c.times.back()) { for (int j = 0; j < cm; ++j) out[j] = val(n - 1, j); return; }
    const int i = static_cast<int>(std::upper_bound(c.times.begin(), c.times.end(), t) - c.times.begin()) - 1;
    const float t0 = c.times[static_cast<std::size_t>(i)], dt = std::max(1e-9f, c.times[static_cast<std::size_t>(i) + 1] - t0), s = (t - t0) / dt;
    if (c.interp == 1) { for (int j = 0; j < cm; ++j) out[j] = val(i, j); return; }
    if (c.interp == 0) {
        if (c.path == 1) { const Quaternion q = slerp({val(i,0),val(i,1),val(i,2),val(i,3)}, {val(i+1,0),val(i+1,1),val(i+1,2),val(i+1,3)}, s); out[0]=q.x; out[1]=q.y; out[2]=q.z; out[3]=q.w; }
        else for (int j = 0; j < cm; ++j) out[j] = val(i, j) + (val(i + 1, j) - val(i, j)) * s;
        return;
    }
    const float s2 = s * s, s3 = s2 * s, h00 = 2*s3 - 3*s2 + 1, h10 = s3 - 2*s2 + s, h01 = -2*s3 + 3*s2, h11 = s3 - s2;
    for (int j = 0; j < cm; ++j) {
        const float p0 = static_cast<float>(c.vals[(static_cast<std::size_t>(i) * 3 + 1) * static_cast<std::size_t>(cm) + static_cast<std::size_t>(j)]);
        const float m0 = static_cast<float>(c.vals[(static_cast<std::size_t>(i) * 3 + 2) * static_cast<std::size_t>(cm) + static_cast<std::size_t>(j)]);
        const float m1 = static_cast<float>(c.vals[(static_cast<std::size_t>(i + 1) * 3 + 0) * static_cast<std::size_t>(cm) + static_cast<std::size_t>(j)]);
        const float p1 = static_cast<float>(c.vals[(static_cast<std::size_t>(i + 1) * 3 + 1) * static_cast<std::size_t>(cm) + static_cast<std::size_t>(j)]);
        out[j] = h00*p0 + h10*dt*m0 + h01*p1 + h11*dt*m1;
    }
    if (c.path == 1) { const Quaternion q = normalize({out[0],out[1],out[2],out[3]}); out[0]=q.x; out[1]=q.y; out[2]=q.z; out[3]=q.w; }
}

struct Node { std::string name; int parent = -1; std::vector<int> children; BoneTransform local; int mesh = -1, skin = -1; };

} // namespace gltf_detail

inline GltfImportResult importGltfFromMemory(const std::uint8_t* data, std::size_t size, const std::filesystem::path& baseDir = {}) {
    using namespace gltf_detail;
    GltfImportResult res;
    auto fail = [&](const std::string& m) { res.ok = false; res.error = m; return res; };
    if (!data || size < 2) return fail("empty glTF input");
    const char* jsonBegin = reinterpret_cast<const char*>(data); std::size_t jsonLen = size;
    std::vector<std::uint8_t> glbBin; bool hasGlbBin = false;
    auto rd32 = [&](std::size_t o) { std::uint32_t v; std::memcpy(&v, data + o, 4); return v; };
    if (size >= 12 && rd32(0) == 0x46546C67u) {                 // "glTF"
        if (rd32(4) != 2u) return fail("unsupported GLB version");
        const std::size_t total = std::min<std::size_t>(rd32(8), size);
        std::size_t pos = 12; bool haveJson = false;
        while (pos + 8 <= total) {
            const std::size_t len = rd32(pos), type = rd32(pos + 4);
            if (len > total - pos - 8) return fail("GLB chunk exceeds file size");
            if (type == 0x4E4F534Au && !haveJson) { jsonBegin = reinterpret_cast<const char*>(data + pos + 8); jsonLen = len; haveJson = true; }
            else if (type == 0x004E4942u && !hasGlbBin) { glbBin.assign(data + pos + 8, data + pos + 8 + len); hasGlbBin = true; }
            pos += 8 + ((len + 3) & ~static_cast<std::size_t>(3));
        }
        if (!haveJson) return fail("GLB has no JSON chunk");
    }
    Json root; std::string err;
    if (!JsonParser(jsonBegin, jsonBegin + jsonLen).parse(root, err)) return fail("JSON: " + err);
    if (root.t != Json::T::Obj) return fail("glTF root is not an object");

    // buffers
    Buffers bufs;
    if (const Json* bs = jarr(root.get("buffers"))) {
        for (std::size_t i = 0; i < bs->a.size(); ++i) {
            const std::string uri = jstr(bs->a[i].get("uri")); std::vector<std::uint8_t> b;
            if (uri.empty()) { if (i == 0 && hasGlbBin) b = glbBin; else return fail("buffer without uri and no GLB binary chunk"); }
            else if (uri.rfind("data:", 0) == 0) {
                const std::size_t comma = uri.find("base64,");
                if (comma == std::string::npos || !base64Decode(std::string_view(uri).substr(comma + 7), b)) return fail("bad data URI buffer");
            } else {
                const std::string rel = percentDecode(uri);
                if (!safeRelativeUri(rel)) return fail("unsafe buffer uri rejected: " + uri);
                std::ifstream f(baseDir / rel, std::ios::binary);
                if (!f) return fail("cannot open buffer file: " + rel);
                b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            }
            bufs.push_back(std::move(b));
        }
    }

    // nodes
    std::vector<Node> nodes;
    if (const Json* ns = jarr(root.get("nodes"))) {
        for (const Json& j : ns->a) {
            Node n; n.name = jstr(j.get("name")); n.mesh = static_cast<int>(jnum(j.get("mesh"), -1)); n.skin = static_cast<int>(jnum(j.get("skin"), -1));
            if (const Json* m = jarr(j.get("matrix")); m && m->a.size() == 16) { AnimMat4 mat; for (int k = 0; k < 16; ++k) mat.m[k] = static_cast<float>(jnum(&m->a[static_cast<std::size_t>(k)])); n.local = transformFromMatrix(mat); }
            else {
                if (const Json* t = jarr(j.get("translation")); t && t->a.size() == 3) n.local.translation = {static_cast<float>(jnum(&t->a[0])), static_cast<float>(jnum(&t->a[1])), static_cast<float>(jnum(&t->a[2]))};
                if (const Json* r = jarr(j.get("rotation")); r && r->a.size() == 4) n.local.rotation = normalize({static_cast<float>(jnum(&r->a[0])), static_cast<float>(jnum(&r->a[1])), static_cast<float>(jnum(&r->a[2])), static_cast<float>(jnum(&r->a[3]))});
                if (const Json* s = jarr(j.get("scale")); s && s->a.size() == 3) n.local.scale = {static_cast<float>(jnum(&s->a[0])), static_cast<float>(jnum(&s->a[1])), static_cast<float>(jnum(&s->a[2]))};
            }
            if (const Json* ch = jarr(j.get("children"))) for (const Json& c : ch->a) n.children.push_back(static_cast<int>(jnum(&c, -1)));
            nodes.push_back(std::move(n));
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) for (int c : nodes[i].children) if (c >= 0 && c < static_cast<int>(nodes.size())) nodes[static_cast<std::size_t>(c)].parent = static_cast<int>(i);
    }
    auto globalOf = [&](int node) {
        AnimMat4 g; int guard = static_cast<int>(nodes.size()) + 1;
        for (int n = node; n >= 0 && guard-- > 0; n = nodes[static_cast<std::size_t>(n)].parent) g = matFromTransform(nodes[static_cast<std::size_t>(n)].local) * g;
        return g;
    };

    // skin -> skeleton
    const Json* skins = jarr(root.get("skins"));
    if (!skins || skins->a.empty()) return fail("glTF has no skin (no skeleton to import)");
    if (skins->a.size() > 1) res.warnings.push_back("file has several skins; only skin 0 is imported");
    const Json& skin = skins->a[0];
    std::vector<int> jointNodes;
    if (const Json* js = jarr(skin.get("joints"))) for (const Json& j : js->a) jointNodes.push_back(static_cast<int>(jnum(&j, -1)));
    if (jointNodes.empty()) return fail("skin has no joints");
    std::unordered_map<int, std::size_t> slotOfNode;
    for (std::size_t s = 0; s < jointNodes.size(); ++s) {
        if (jointNodes[s] < 0 || jointNodes[s] >= static_cast<int>(nodes.size())) return fail("skin joint refers to a missing node");
        if (!slotOfNode.emplace(jointNodes[s], s).second) return fail("skin lists the same joint twice");
    }
    auto jointParentOf = [&](int node) { int guard = static_cast<int>(nodes.size()) + 1; for (int p = nodes[static_cast<std::size_t>(node)].parent; p >= 0 && guard-- > 0; p = nodes[static_cast<std::size_t>(p)].parent) if (slotOfNode.count(p)) return p; return -1; };
    std::vector<int> order; std::unordered_map<int, int> boneOfNode;
    std::function<void(int, int)> emit = [&](int node, int depth) {
        if (boneOfNode.count(node) || depth > static_cast<int>(jointNodes.size()) + 1) return;
        const int jp = jointParentOf(node);
        if (jp >= 0) emit(jp, depth + 1);
        boneOfNode[node] = static_cast<int>(order.size()); order.push_back(node);
    };
    for (int n : jointNodes) emit(n, 0);
    std::vector<int> slotToBone(jointNodes.size());
    for (std::size_t s = 0; s < jointNodes.size(); ++s) slotToBone[s] = boneOfNode[jointNodes[s]];
    for (int node : order) {
        const Node& nd = nodes[static_cast<std::size_t>(node)];
        Bone b; b.name = nd.name.empty() ? "joint_" + std::to_string(node) : nd.name;
        const int jp = jointParentOf(node);
        b.parent = jp >= 0 ? boneOfNode[jp] : -1;
        if (jp >= 0 && nd.parent != jp) {
            b.bindPose = transformFromMatrix(inverseAffine(globalOf(jp)) * globalOf(node));
            res.warnings.push_back("joint '" + b.name + "' has non-joint nodes between it and its parent joint; animated tracks for it may be inaccurate");
        } else {
            b.bindPose = nd.local;
            if (jp < 0 && nd.parent >= 0) {
                const AnimMat4 g = globalOf(nd.parent); bool identity = true;
                for (int k = 0; k < 16; ++k) if (std::fabs(g.m[k] - AnimMat4{}.m[k]) > 1e-5f) identity = false;
                if (!identity) res.warnings.push_back("root joint '" + b.name + "' sits under a non-identity node transform; that transform is not part of the skeleton");
            }
        }
        res.bones.push_back(std::move(b));
    }
    Skeleton skeleton(res.bones);
    res.inverseBind = computeInverseBindPose(skeleton);
    if (const Json* ibm = skin.get("inverseBindMatrices")) {
        Acc acc;
        if (!readAccessor(root, bufs, static_cast<int>(jnum(ibm, -1)), acc, err)) return fail("inverseBindMatrices: " + err);
        if (acc.comps == 16 && acc.count == jointNodes.size())
            for (std::size_t s = 0; s < jointNodes.size(); ++s) { AnimMat4 m; for (int k = 0; k < 16; ++k) m.m[k] = static_cast<float>(acc.v[s * 16 + static_cast<std::size_t>(k)]); res.inverseBind[static_cast<std::size_t>(slotToBone[s])] = m; }
        else res.warnings.push_back("inverseBindMatrices size mismatch; bind-pose inverse used instead");
    }

    // animations
    if (const Json* anims = jarr(root.get("animations"))) {
        int ignored = 0, morph = 0;
        for (std::size_t ai = 0; ai < anims->a.size(); ++ai) {
            const Json& an = anims->a[ai];
            AnimationClip clip; clip.name = jstr(an.get("name")); if (clip.name.empty()) clip.name = "clip_" + std::to_string(ai);
            clip.tracks.resize(res.bones.size());
            const Json* samplers = jarr(an.get("samplers")); const Json* channels = jarr(an.get("channels"));
            std::vector<std::vector<Chan>> perBone(res.bones.size());
            float duration = 0;
            if (samplers && channels) for (const Json& ch : channels->a) {
                const Json* target = ch.get("target"); if (!target) continue;
                const std::string path = jstr(target->get("path"));
                if (path == "weights") { ++morph; continue; }
                const int pathId = path == "translation" ? 0 : path == "rotation" ? 1 : path == "scale" ? 2 : -1;
                const auto bit = boneOfNode.find(static_cast<int>(jnum(target->get("node"), -1)));
                if (pathId < 0 || bit == boneOfNode.end()) { ++ignored; continue; }
                const int si = static_cast<int>(jnum(ch.get("sampler"), -1));
                if (si < 0 || si >= static_cast<int>(samplers->a.size())) { res.warnings.push_back("channel refers to a missing sampler in '" + clip.name + "'"); continue; }
                const Json& sm = samplers->a[static_cast<std::size_t>(si)];
                Acc tin, vout;
                if (!readAccessor(root, bufs, static_cast<int>(jnum(sm.get("input"), -1)), tin, err) || !readAccessor(root, bufs, static_cast<int>(jnum(sm.get("output"), -1)), vout, err)) return fail("animation '" + clip.name + "': " + err);
                Chan c; c.path = pathId; c.comps = pathId == 1 ? 4 : 3;
                const std::string interp = jstr(sm.get("interpolation"));
                c.interp = interp == "STEP" ? 1 : interp == "CUBICSPLINE" ? 2 : 0;
                for (double t : tin.v) c.times.push_back(static_cast<float>(t));
                c.vals = std::move(vout.v);
                const std::size_t need = c.times.size() * static_cast<std::size_t>(c.comps) * (c.interp == 2 ? 3u : 1u);
                bool ascending = std::is_sorted(c.times.begin(), c.times.end());
                if (c.times.empty() || vout.comps != c.comps || c.vals.size() < need || !ascending) { res.warnings.push_back("invalid animation channel skipped in '" + clip.name + "'"); continue; }
                duration = std::max(duration, c.times.back());
                perBone[static_cast<std::size_t>(bit->second)].push_back(std::move(c));
            }
            for (std::size_t b = 0; b < perBone.size(); ++b) {
                if (perBone[b].empty()) continue;
                const Chan* use[3] = {nullptr, nullptr, nullptr};
                for (const Chan& c : perBone[b]) use[c.path] = &c;     // last channel wins on duplicates
                std::vector<float> times;
                for (const Chan* c : use) {
                    if (!c) continue;
                    for (std::size_t k = 0; k < c->times.size(); ++k) {
                        times.push_back(c->times[k]);
                        if (k + 1 >= c->times.size()) continue;
                        const float a = c->times[k], z = c->times[k + 1];
                        if (c->interp == 1 && z - a > 2e-4f) times.push_back(z - 1e-4f);
                        if (c->interp == 2) for (int sub = 1; sub < 8; ++sub) times.push_back(a + (z - a) * static_cast<float>(sub) / 8.0f);
                    }
                }
                std::sort(times.begin(), times.end());
                times.erase(std::unique(times.begin(), times.end(), [](float x, float y) { return std::fabs(x - y) < 1e-6f; }), times.end());
                for (float t : times) {
                    AnimationKey key; key.time = t; key.value = res.bones[b].bindPose; float v[4];
                    if (use[0]) { evalChan(*use[0], t, v); key.value.translation = {v[0], v[1], v[2]}; }
                    if (use[1]) { evalChan(*use[1], t, v); key.value.rotation = {v[0], v[1], v[2], v[3]}; }
                    if (use[2]) { evalChan(*use[2], t, v); key.value.scale = {v[0], v[1], v[2]}; }
                    clip.tracks[b].keys.push_back(key);
                }
            }
            clip.duration = duration;
            res.clips.push_back(std::move(clip));
        }
        if (ignored) res.warnings.push_back(std::to_string(ignored) + " animation channel(s) target nodes outside the skeleton and were ignored");
        if (morph) res.warnings.push_back(std::to_string(morph) + " morph-target weight channel(s) ignored (not supported yet)");
    }

    // skinned meshes
    if (const Json* meshes = jarr(root.get("meshes"))) {
        std::size_t skipped = 0; bool badJoint = false;
        for (std::size_t mi = 0; mi < meshes->a.size(); ++mi) {
            const Json* prims = jarr(meshes->a[mi].get("primitives")); if (!prims) continue;
            for (std::size_t pi = 0; pi < prims->a.size(); ++pi) {
                const Json& pr = prims->a[pi]; const Json* at = pr.get("attributes");
                if (!at || !at->get("POSITION") || !at->get("JOINTS_0") || !at->get("WEIGHTS_0") || static_cast<int>(jnum(pr.get("mode"), 4)) != 4) { ++skipped; continue; }
                Acc pos, jn, wt, nrm, uv, idx;
                if (!readAccessor(root, bufs, static_cast<int>(jnum(at->get("POSITION"), -1)), pos, err) || !readAccessor(root, bufs, static_cast<int>(jnum(at->get("JOINTS_0"), -1)), jn, err) || !readAccessor(root, bufs, static_cast<int>(jnum(at->get("WEIGHTS_0"), -1)), wt, err)) return fail("mesh: " + err);
                if (pos.comps != 3 || jn.comps != 4 || wt.comps != 4 || jn.count != pos.count || wt.count != pos.count) { res.warnings.push_back("mesh primitive with inconsistent attribute sizes skipped"); ++skipped; continue; }
                const bool hasN = at->get("NORMAL") && readAccessor(root, bufs, static_cast<int>(jnum(at->get("NORMAL"), -1)), nrm, err) && nrm.comps == 3 && nrm.count == pos.count;
                const bool hasUv = at->get("TEXCOORD_0") && readAccessor(root, bufs, static_cast<int>(jnum(at->get("TEXCOORD_0"), -1)), uv, err) && uv.comps == 2 && uv.count == pos.count;
                SkinnedMeshData md; md.name = jstr(meshes->a[mi].get("name")); if (md.name.empty()) md.name = "mesh_" + std::to_string(mi); if (prims->a.size() > 1) md.name += "_" + std::to_string(pi);
                for (std::size_t v = 0; v < pos.count; ++v) {
                    SkinnedVertex sv; sv.position = {static_cast<float>(pos.v[v*3]), static_cast<float>(pos.v[v*3+1]), static_cast<float>(pos.v[v*3+2])};
                    if (hasN) sv.normal = {static_cast<float>(nrm.v[v*3]), static_cast<float>(nrm.v[v*3+1]), static_cast<float>(nrm.v[v*3+2])};
                    if (hasUv) { sv.u = static_cast<float>(uv.v[v*2]); sv.v = static_cast<float>(uv.v[v*2+1]); }
                    float sum = 0;
                    for (int k = 0; k < 4; ++k) {
                        const std::size_t slot = static_cast<std::size_t>(jn.v[v*4 + static_cast<std::size_t>(k)]);
                        if (slot >= slotToBone.size()) { badJoint = true; sv.joints[k] = 0; sv.weights[k] = 0; continue; }
                        sv.joints[k] = static_cast<std::uint16_t>(slotToBone[slot]);
                        sv.weights[k] = std::max(0.0f, static_cast<float>(wt.v[v*4 + static_cast<std::size_t>(k)])); sum += sv.weights[k];
                    }
                    if (sum > 1e-6f) for (float& w : sv.weights) w /= sum; else { sv.weights[0] = 1; }
                    md.vertices.push_back(sv);
                }
                if (const Json* ii = pr.get("indices")) {
                    if (!readAccessor(root, bufs, static_cast<int>(jnum(ii, -1)), idx, err)) return fail("mesh indices: " + err);
                    for (double i : idx.v) { if (i < 0 || i >= static_cast<double>(pos.count)) return fail("mesh index out of range"); md.indices.push_back(static_cast<std::uint32_t>(i)); }
                } else for (std::size_t i = 0; i < pos.count; ++i) md.indices.push_back(static_cast<std::uint32_t>(i));
                if (md.indices.size() % 3 != 0) { res.warnings.push_back("mesh '" + md.name + "' index count is not a multiple of 3"); md.indices.resize(md.indices.size() / 3 * 3); }
                res.meshes.push_back(std::move(md));
            }
        }
        if (skipped) res.warnings.push_back(std::to_string(skipped) + " non-skinned or unsupported mesh primitive(s) skipped");
        if (badJoint) res.warnings.push_back("some vertices referenced joints outside the skin; their weights were zeroed");
    }
    res.ok = true;
    return res;
}

inline GltfImportResult importGltfFile(const std::filesystem::path& path) {
    std::error_code ec; const auto sz = std::filesystem::file_size(path, ec);
    GltfImportResult bad;
    if (ec) { bad.error = "cannot stat " + path.string(); return bad; }
    if (sz > 512ull * 1024 * 1024) { bad.error = "file larger than 512 MB"; return bad; }
    std::ifstream f(path, std::ios::binary);
    if (!f) { bad.error = "cannot open " + path.string(); return bad; }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(sz));
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(sz));
    return importGltfFromMemory(bytes.data(), bytes.size(), path.parent_path());
}

} // namespace anvil
