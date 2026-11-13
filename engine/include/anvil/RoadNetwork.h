#pragma once
// Road graph (nodes + straight segments with lanes) and a lightweight traffic / pedestrian simulation.
// Right-hand traffic. Lanes of a->b direction lie to the right of the a->b centreline, b->a lanes to its left.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
namespace anvil {

struct RoadNode { uint32_t id = 0; glm::vec3 pos{0}; };
struct RoadSegment { uint32_t id = 0, a = 0, b = 0; int lanesFwd = 1, lanesBack = 1; float laneWidth = 3.5f, speedLimit = 13.9f; };

class RoadNetwork {
public:
    std::vector<RoadNode> nodes;
    std::vector<RoadSegment> segments;

    uint32_t addNode(const glm::vec3& p) { nodes.push_back({next_++, p}); dirty_ = true; return nodes.back().id; }
    uint32_t addSegment(uint32_t a, uint32_t b, int lf, int lb, float lw, float speed) {
        if (a == b || !node(a) || !node(b) || (lf <= 0 && lb <= 0)) return 0;
        for (const auto& s : segments) if ((s.a == a && s.b == b) || (s.a == b && s.b == a)) return 0;
        segments.push_back({next_++, a, b, std::max(lf, 0), std::max(lb, 0), lw, speed}); dirty_ = true; return segments.back().id;
    }
    void removeSegment(uint32_t id) { segments.erase(std::remove_if(segments.begin(), segments.end(), [&](auto& s) { return s.id == id; }), segments.end()); dirty_ = true; }
    void removeNode(uint32_t id) {
        segments.erase(std::remove_if(segments.begin(), segments.end(), [&](auto& s) { return s.a == id || s.b == id; }), segments.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](auto& n) { return n.id == id; }), nodes.end()); dirty_ = true;
    }
    const RoadNode* node(uint32_t id) const { reindex(); auto it = nodeIdx_.find(id); return it == nodeIdx_.end() ? nullptr : &nodes[it->second]; }
    const RoadSegment* segment(uint32_t id) const { reindex(); auto it = segIdx_.find(id); return it == segIdx_.end() ? nullptr : &segments[it->second]; }
    uint32_t nearestNode(const glm::vec3& p, float maxDist) const {
        uint32_t best = 0; float bd = maxDist * maxDist;
        for (const auto& n : nodes) { glm::vec3 d = n.pos - p; d.y = 0; float q = glm::dot(d, d); if (q <= bd) { bd = q; best = n.id; } }
        return best;
    }
    std::vector<uint32_t> segmentsAt(uint32_t nodeId) const { std::vector<uint32_t> r; for (const auto& s : segments) if (s.a == nodeId || s.b == nodeId) r.push_back(s.id); return r; }
    float length(const RoadSegment& s) const { const RoadNode *a = node(s.a), *b = node(s.b); if (!a || !b) return 0; glm::vec3 d = b->pos - a->pos; d.y = 0; return glm::length(d); }
    glm::vec3 dir(const RoadSegment& s) const { const RoadNode *a = node(s.a), *b = node(s.b); if (!a || !b) return {0, 0, 1}; glm::vec3 d = b->pos - a->pos; d.y = 0; float l = glm::length(d); return l > 1e-4f ? d / l : glm::vec3(0, 0, 1); }
    static glm::vec3 rightOf(const glm::vec3& d) { return {-d.z, 0.0f, d.x}; }

    bool save(const std::filesystem::path& p) const {
        std::error_code ec; std::filesystem::create_directories(p.parent_path(), ec);
        std::ofstream f(p); if (!f) return false; f << "ROADS 1\n";
        for (auto& n : nodes) f << "node " << n.id << ' ' << n.pos.x << ' ' << n.pos.y << ' ' << n.pos.z << "\n";
        for (auto& s : segments) f << "seg " << s.id << ' ' << s.a << ' ' << s.b << ' ' << s.lanesFwd << ' ' << s.lanesBack << ' ' << s.laneWidth << ' ' << s.speedLimit << "\n";
        return true;
    }
    bool load(const std::filesystem::path& p) {
        std::ifstream f(p); if (!f) return false; std::string tag; int ver; if (!(f >> tag >> ver) || tag != "ROADS") return false;
        nodes.clear(); segments.clear(); next_ = 1; dirty_ = true;
        while (f >> tag) {
            if (tag == "node") { RoadNode n; f >> n.id >> n.pos.x >> n.pos.y >> n.pos.z; nodes.push_back(n); next_ = std::max(next_, n.id + 1); }
            else if (tag == "seg") { RoadSegment s; f >> s.id >> s.a >> s.b >> s.lanesFwd >> s.lanesBack >> s.laneWidth >> s.speedLimit; segments.push_back(s); next_ = std::max(next_, s.id + 1); }
            else break;
        }
        dirty_ = true; return true;
    }
private:
    void reindex() const {
        if (!dirty_) return; nodeIdx_.clear(); segIdx_.clear();
        for (size_t i = 0; i < nodes.size(); ++i) nodeIdx_[nodes[i].id] = i;
        for (size_t i = 0; i < segments.size(); ++i) segIdx_[segments[i].id] = i;
        dirty_ = false;
    }
    uint32_t next_ = 1; mutable bool dirty_ = true;
    mutable std::unordered_map<uint32_t, size_t> nodeIdx_, segIdx_;
};

struct TrafficAgent {
    uint32_t id = 0; bool ped = false, dead = false; uint32_t seg = 0; bool fwd = true;
    float p = 0, speed = 0, desired = 10, wait = 0; int lane = 0, side = 1; uint32_t heldNode = 0, color = 0;
    glm::vec3 pos{0}, heading{0, 0, 1};
};

class TrafficSim {
public:
    int maxCars = 40, maxPeds = 60; float minSpawnDist = 25.0f;
    std::vector<TrafficAgent> agents;
    std::function<bool(const glm::vec3&)> allowed;   // nullptr = everywhere; used to keep agents inside loaded world cells
    void clear() { agents.clear(); reserved_.clear(); }
    int carCount() const { int n = 0; for (auto& a : agents) if (!a.ped) ++n; return n; }
    int pedCount() const { int n = 0; for (auto& a : agents) if (a.ped) ++n; return n; }

    void update(const RoadNetwork& net, float dt, const glm::vec3& center, float spawnRadius, float despawnRadius) {
        dt = std::min(dt, 0.1f);
        for (auto& a : agents) step(net, a, dt);
        for (auto& a : agents) {   // despawn
            if (a.dead) continue; glm::vec3 d = a.pos - center; d.y = 0;
            if (glm::length(d) > despawnRadius || (allowed && !allowed(a.pos))) a.dead = true;
        }
        for (auto& a : agents) if (a.dead) release(a);
        agents.erase(std::remove_if(agents.begin(), agents.end(), [](auto& a) { return a.dead; }), agents.end());
        for (int i = 0; i < 2; ++i) trySpawn(net, center, spawnRadius);
    }
private:
    uint32_t rng_ = 2463534242u, nextId_ = 1; std::unordered_map<uint32_t, uint32_t> reserved_;   // node -> car id
    float rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return float(rng_ & 0xFFFFFF) / float(0x1000000); }
    void release(TrafficAgent& a) { if (a.heldNode) { auto it = reserved_.find(a.heldNode); if (it != reserved_.end() && it->second == a.id) reserved_.erase(it); a.heldNode = 0; } }
    void place(const RoadNetwork& net, TrafficAgent& a) {
        const RoadSegment* s = net.segment(a.seg); if (!s) { a.dead = true; return; }
        const RoadNode *A = net.node(s->a), *B = net.node(s->b); float len = std::max(net.length(*s), 1e-3f);
        glm::vec3 d = net.dir(*s), travel = a.fwd ? d : -d; float u = std::clamp((a.fwd ? a.p : len - a.p) / len, 0.0f, 1.0f);
        glm::vec3 pt = glm::mix(A->pos, B->pos, u), off(0);
        if (!a.ped) off = RoadNetwork::rightOf(travel) * ((a.lane + 0.5f) * s->laneWidth);
        else { glm::vec3 r = RoadNetwork::rightOf(d); off = a.side > 0 ? r * (s->lanesFwd * s->laneWidth + 1.2f) : -r * (s->lanesBack * s->laneWidth + 1.2f); }
        a.pos = pt + off; a.heading = travel;
    }
    void step(const RoadNetwork& net, TrafficAgent& a, float dt) {
        const RoadSegment* s = net.segment(a.seg); if (!s) { a.dead = true; return; }
        const float len = net.length(*s); float target = a.desired, gap = 1e9f, leaderSpeed = a.desired;
        for (const auto& o : agents) {
            if (&o == &a || o.dead || o.seg != a.seg || o.fwd != a.fwd || o.ped != a.ped || (a.ped ? o.side != a.side : o.lane != a.lane)) continue;
            float d = o.p - a.p; if (d > 0 && d < gap) { gap = d; leaderSpeed = o.speed; }
        }
        const float safe = 5.0f + a.speed * 1.3f;
        if (gap < safe) target = std::min(target, leaderSpeed * (gap / safe)); if (gap < 3.5f) target = 0;
        const float dEnd = len - a.p; const uint32_t endNode = a.fwd ? s->b : s->a;
        if (!a.ped) {
            if (dEnd < 14.0f) target = std::min(target, std::max(4.0f, a.desired * 0.5f));
            if (dEnd < 9.0f && a.heldNode != endNode) {
                auto it = reserved_.find(endNode);
                if (it == reserved_.end() || it->second == a.id || a.wait > 6.0f) { reserved_[endNode] = a.id; a.heldNode = endNode; a.wait = 0; }
                else { target = dEnd <= 6.5f ? 0.0f : std::min(target, 3.0f); if (a.speed < 0.5f) a.wait += dt; }
            }
        }
        a.speed = a.speed < target ? std::min(target, a.speed + 3.0f * dt) : std::max(target, a.speed - 9.0f * dt);
        a.p += a.speed * dt;
        if (a.p >= len) {
            auto cands = net.segmentsAt(endNode); std::vector<std::pair<uint32_t, bool>> opts;
            for (uint32_t sid : cands) {
                if (sid == a.seg && cands.size() > 1) continue; const RoadSegment* s2 = net.segment(sid); if (!s2) continue;
                bool f2 = s2->a == endNode; int lanes = f2 ? s2->lanesFwd : s2->lanesBack; if (!a.ped && lanes <= 0) continue; opts.push_back({sid, f2});
            }
            float over = a.p - len;
            if (opts.empty()) {   // dead end: turn around if the opposite direction has lanes
                int lanes = a.fwd ? s->lanesBack : s->lanesFwd; if (!a.ped && lanes <= 0) { a.dead = true; return; }
                a.fwd = !a.fwd; a.p = 0; if (!a.ped) a.lane = std::min(a.lane, std::max(lanes - 1, 0));
            } else {
                auto pick = opts[size_t(rnd() * opts.size()) % opts.size()]; const RoadSegment* s2 = net.segment(pick.first);
                a.seg = pick.first; a.fwd = pick.second; a.p = std::max(over, 0.0f);
                if (!a.ped) a.lane = std::min(a.lane, std::max((a.fwd ? s2->lanesFwd : s2->lanesBack) - 1, 0));
            }
        }
        place(net, a);
        if (a.heldNode) { if (const RoadNode* n = net.node(a.heldNode)) { glm::vec3 d = a.pos - n->pos; d.y = 0; if (glm::length(d) >= 10.0f) release(a); } }
    }
    void trySpawn(const RoadNetwork& net, const glm::vec3& center, float radius) {
        if (net.segments.empty()) return;
        const int cars = carCount(), peds = pedCount(); bool ped;
        if (cars < maxCars && (peds >= maxPeds || rnd() < 0.5f)) ped = false; else if (peds < maxPeds) ped = true; else return;
        for (int attempt = 0; attempt < 12; ++attempt) {
            const RoadSegment& s = net.segments[size_t(rnd() * net.segments.size()) % net.segments.size()];
            TrafficAgent a; a.id = nextId_; a.ped = ped; a.seg = s.id; a.fwd = rnd() < 0.5f;
            if (!ped) { int lanes = a.fwd ? s.lanesFwd : s.lanesBack; if (lanes <= 0) { a.fwd = !a.fwd; lanes = a.fwd ? s.lanesFwd : s.lanesBack; } if (lanes <= 0) continue; a.lane = int(rnd() * lanes) % lanes; }
            else a.side = rnd() < 0.5f ? 1 : -1;
            a.p = rnd() * net.length(s); place(net, a); if (a.dead) continue;
            glm::vec3 d = a.pos - center; d.y = 0; float dist = glm::length(d);
            if (dist > radius || dist < minSpawnDist || (allowed && !allowed(a.pos))) continue;
            bool clear = true; for (const auto& o : agents) if (o.seg == a.seg && o.fwd == a.fwd && o.ped == a.ped && std::abs(o.p - a.p) < 10.0f) { clear = false; break; }
            if (!clear) continue;
            a.desired = ped ? 1.2f + rnd() * 0.6f : s.speedLimit * (0.75f + rnd() * 0.2f); a.color = uint32_t(rnd() * 1000.0f);
            ++nextId_; agents.push_back(a); return;
        }
    }
};
} // namespace anvil
