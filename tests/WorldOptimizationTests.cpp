#include "anvil/WorldOptimization.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>

int main() {
    anvil::LodSelector lod({{0, 0.0f}, {1, 40.0f}, {2, 100.0f}}, 0.10f);
    assert(lod.select(10.0f) == 0);
    assert(lod.select(50.0f) == 1);
    assert(lod.select(43.0f, 0) == 0); // retain old LOD inside hysteresis band
    assert(lod.select(60.0f, 1) == 1);
    assert(lod.select(130.0f, 1) == 2);
    assert(lod.select(95.0f, 2) == 2); // no rapid switch near 100
    assert(lod.select(80.0f, 2) == 1);
    assert(lod.select(std::numeric_limits<float>::quiet_NaN()) == 0);
    anvil::WorldPartition invalidPartition(std::numeric_limits<float>::quiet_NaN(), 100000, 100000);
    assert(invalidPartition.update(std::numeric_limits<float>::infinity(), 0, 0).empty());

    const auto transition = anvil::selectLodTransition(lod, 35.0f, 0, 8.0f);
    assert(transition.primary == 0 && transition.secondary == 1);
    assert(transition.secondaryWeight > 0.0f && transition.secondaryWeight < 1.0f);
    const auto noTransition = anvil::selectLodTransition(lod, 10.0f, 0, 8.0f);
    assert(noTransition.primary == 0 && noTransition.secondary == 0);

    anvil::WorldPartition partition(128.0f, 1, 2);
    const auto initialLoads = partition.update(0.0f, 0.0f, 0.0f);
    assert(initialLoads.size() == 27);
    assert(partition.residentCount() == 27);
    assert(partition.update(0.0f, 0.0f, 0.0f).empty());
    assert(partition.isResident({0, 0, 0}));
    const auto nextLoads = partition.update(128.0f, 0.0f, 0.0f);
    assert(nextLoads.size() == 9);
    assert(partition.isResident({-1, 0, 0})); // unload hysteresis retains nearby cells
    partition.update(1024.0f, 0.0f, 0.0f);
    assert(!partition.isResident({-1, 0, 0}));

    const auto path = std::filesystem::temp_directory_path() / "anvil_stream_test.bin";
    { std::ofstream f(path, std::ios::binary); f << "ANVIL_STREAM_OK"; }
    anvil::AsyncAssetStreamer streamer;
    assert(streamer.request(path, 5));
    assert(!streamer.request(path, 1)); // duplicate pending request is ignored
    std::vector<anvil::StreamedAsset> results;
    for (int i = 0; i < 100 && results.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        results = streamer.pollCompleted();
    }
    assert(results.size() == 1);
    assert(results[0].error.empty());
    assert(std::string(results[0].bytes.begin(), results[0].bytes.end()) == "ANVIL_STREAM_OK");
    streamer.stop();
    anvil::AsyncAssetStreamer limitedStreamer(4);
    assert(limitedStreamer.request(path));
    std::vector<anvil::StreamedAsset> limited;
    for (int i = 0; i < 100 && limited.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        limited = limitedStreamer.pollCompleted();
    }
    assert(limited.size() == 1 && !limited[0].error.empty() && limited[0].bytes.empty());
    limitedStreamer.stop();
    std::filesystem::remove(path);
}
