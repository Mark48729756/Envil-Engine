#pragma once
// ANVIL cutscene model: camera track (Catmull-Rom + ease), entity transform tracks,
// timed events (subtitle / fade / shake / sound), text serialization.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <glm/glm.hpp>
namespace anvil {

struct CutCamKey { float t = 0; glm::vec3 pos{0}; float yaw = -90, pitch = 0, fov = 60; bool smooth = true; };
struct CutXformKey { float t = 0; glm::vec3 pos{0}, rot{0}, scale{1}; };
struct CutEntityTrack { std::string entity; std::vector<CutXformKey> keys; };
enum class CutEventType { Subtitle = 0, Fade = 1, Shake = 2, Sound = 3 };
struct CutEvent { CutEventType type = CutEventType::Subtitle; float t = 0, dur = 2, value = 1; std::string speaker, text; };
struct CamPose { glm::vec3 pos{0}; float yaw = -90, pitch = 0, fov = 60; };

struct Cutscene {
    std::string name = "Cutscene1";
    float duration = 10.0f;
    bool letterbox = true;
    std::vector<CutCamKey> cam;
    std::vector<CutEntityTrack> tracks;
    std::vector<CutEvent> events;

    static float ease(float u) { u = std::clamp(u, 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); }
    static float lerpAngle(float a, float b, float u) { float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f; return a + d * u; }
    static glm::vec3 catmull(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float u) {
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
    }
    void sortAll() {
        std::stable_sort(cam.begin(), cam.end(), [](auto& a, auto& b) { return a.t < b.t; });
        for (auto& tr : tracks) std::stable_sort(tr.keys.begin(), tr.keys.end(), [](auto& a, auto& b) { return a.t < b.t; });
        std::stable_sort(events.begin(), events.end(), [](auto& a, auto& b) { return a.t < b.t; });
    }
    bool evalCamera(float time, CamPose& out) const {
        if (cam.empty()) return false;
        if (cam.size() == 1 || time <= cam.front().t) { auto& k = cam.front(); out = {k.pos, k.yaw, k.pitch, k.fov}; return true; }
        if (time >= cam.back().t) { auto& k = cam.back(); out = {k.pos, k.yaw, k.pitch, k.fov}; return true; }
        size_t i = 0; while (i + 2 < cam.size() && time >= cam[i + 1].t) ++i;
        const CutCamKey &a = cam[i], &b = cam[i + 1];
        const CutCamKey& p0 = cam[i ? i - 1 : i]; const CutCamKey& p3 = cam[i + 2 < cam.size() ? i + 2 : i + 1];
        float u = (time - a.t) / std::max(b.t - a.t, 1e-4f); if (a.smooth) u = ease(u);
        out.pos = catmull(p0.pos, a.pos, b.pos, p3.pos, u);
        out.yaw = lerpAngle(a.yaw, b.yaw, u); out.pitch = a.pitch + (b.pitch - a.pitch) * u; out.fov = a.fov + (b.fov - a.fov) * u;
        return true;
    }
    static bool evalTrack(const CutEntityTrack& tr, float time, CutXformKey& out) {
        if (tr.keys.empty()) return false;
        if (time <= tr.keys.front().t || tr.keys.size() == 1) { out = tr.keys.front(); return true; }
        if (time >= tr.keys.back().t) { out = tr.keys.back(); return true; }
        size_t i = 0; while (i + 2 < tr.keys.size() && time >= tr.keys[i + 1].t) ++i;
        const auto &a = tr.keys[i], &b = tr.keys[i + 1];
        float u = ease((time - a.t) / std::max(b.t - a.t, 1e-4f));
        out.pos = glm::mix(a.pos, b.pos, u); out.scale = glm::mix(a.scale, b.scale, u);
        out.rot = {lerpAngle(a.rot.x, b.rot.x, u), lerpAngle(a.rot.y, b.rot.y, u), lerpAngle(a.rot.z, b.rot.z, u)};
        return true;
    }
    const CutEvent* activeSubtitle(float time) const {
        for (auto& e : events) if (e.type == CutEventType::Subtitle && time >= e.t && time < e.t + e.dur) return &e;
        return nullptr;
    }
    // 0 = clear, 1 = black. Fade event value: 0 = fade in (from black), 1 = fade out (to black); state persists after it ends.
    float fadeAlpha(float time) const {
        const CutEvent* last = nullptr;
        for (auto& e : events) if (e.type == CutEventType::Fade && e.t <= time) last = &e;
        if (!last) return 0.0f;
        float k = std::clamp((time - last->t) / std::max(last->dur, 1e-3f), 0.0f, 1.0f);
        return last->value > 0.5f ? k : 1.0f - k;
    }
    glm::vec3 shakeOffset(float time) const {
        glm::vec3 o{0};
        for (auto& e : events) if (e.type == CutEventType::Shake && time >= e.t && time < e.t + e.dur) {
            float k = (time - e.t) / std::max(e.dur, 1e-3f), a = e.value * (1.0f - k) * 0.08f;
            o += glm::vec3(std::sin(time * 63.0f), std::sin(time * 71.0f + 1.3f), std::sin(time * 57.0f + 2.1f)) * a;
        }
        return o;
    }
    static std::string clean(std::string s) { for (char& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' '; return s; }
    bool save(const std::filesystem::path& path, std::string* err = nullptr) const {
        std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream f(path); if (!f) { if (err) *err = "cannot write file"; return false; }
        f << "CUTSCENE 1\nname\t" << clean(name) << "\nduration\t" << duration << "\nletterbox\t" << (letterbox ? 1 : 0) << "\n";
        for (auto& k : cam) f << "cam\t" << k.t << '\t' << k.pos.x << '\t' << k.pos.y << '\t' << k.pos.z << '\t' << k.yaw << '\t' << k.pitch << '\t' << k.fov << '\t' << (k.smooth ? 1 : 0) << "\n";
        for (auto& tr : tracks) {
            f << "track\t" << clean(tr.entity) << "\n";
            for (auto& k : tr.keys) f << "key\t" << k.t << '\t' << k.pos.x << '\t' << k.pos.y << '\t' << k.pos.z << '\t' << k.rot.x << '\t' << k.rot.y << '\t' << k.rot.z << '\t' << k.scale.x << '\t' << k.scale.y << '\t' << k.scale.z << "\n";
        }
        for (auto& e : events) f << "event\t" << static_cast<int>(e.type) << '\t' << e.t << '\t' << e.dur << '\t' << e.value << '\t' << clean(e.speaker) << '\t' << clean(e.text) << "\n";
        return static_cast<bool>(f);
    }
    bool load(const std::filesystem::path& path, std::string* err = nullptr) {
        std::ifstream f(path); if (!f) { if (err) *err = "file not found"; return false; }
        Cutscene c; std::string line; std::getline(f, line); if (line.rfind("CUTSCENE", 0) != 0) { if (err) *err = "not a cutscene file"; return false; }
        while (std::getline(f, line)) {
            std::vector<std::string> p; std::stringstream ss(line); std::string t; while (std::getline(ss, t, '\t')) p.push_back(t);
            if (line.size() && line.back() == '\t') p.push_back("");
            if (p.empty()) continue;
            auto F = [&](size_t i) { return i < p.size() ? std::strtof(p[i].c_str(), nullptr) : 0.0f; };
            if (p[0] == "name" && p.size() > 1) c.name = p[1];
            else if (p[0] == "duration") c.duration = std::max(F(1), 0.1f);
            else if (p[0] == "letterbox") c.letterbox = F(1) > 0.5f;
            else if (p[0] == "cam") c.cam.push_back({F(1), {F(2), F(3), F(4)}, F(5), F(6), F(7), F(8) > 0.5f});
            else if (p[0] == "track" && p.size() > 1) c.tracks.push_back({p[1], {}});
            else if (p[0] == "key" && !c.tracks.empty()) c.tracks.back().keys.push_back({F(1), {F(2), F(3), F(4)}, {F(5), F(6), F(7)}, {F(8), F(9), F(10)}});
            else if (p[0] == "event") c.events.push_back({static_cast<CutEventType>(std::clamp(static_cast<int>(F(1)), 0, 3)), F(2), F(3), F(4), p.size() > 5 ? p[5] : "", p.size() > 6 ? p[6] : ""});
        }
        c.sortAll(); *this = std::move(c); return true;
    }
};
} // namespace anvil
