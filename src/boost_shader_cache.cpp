// Boost: ANGLE async shader disk & memory cache (Pipeline Cache)
// ANGLE has a built-in blob cache callback (eglSetBlobCacheFuncsANDROID).
// We persist compiled D3D shaders in memory and asynchronously write to disk,
// eliminating all render-thread disk I/O hitches during level play.

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "config.hpp"
#include "angle_loader.hpp"

typedef void  (*EglSetBlobCacheFunc)(void*, void(*set)(const void*, long, const void*, long), long(*get)(const void*, long, void*, long));

namespace {

struct WriteTask {
    std::string path;
    std::vector<uint8_t> data;
};

static std::string g_cacheDir;
static std::mutex g_cacheMutex;
static std::unordered_map<std::string, std::vector<uint8_t>> g_ramCache;

static std::mutex g_queueMutex;
static std::condition_variable g_queueCv;
static std::vector<WriteTask> g_writeQueue;
static std::thread g_workerThread;
static std::atomic<bool> g_stopWorker{false};

static std::string keyToPath(const void* key, long keySize) {
    // hash the key bytes into a hex filename
    unsigned int hash = 5381;
    const auto* k = (const unsigned char*)key;
    for (long i = 0; i < keySize; i++) {
        hash = ((hash << 5) + hash) + k[i];
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%08x.bin", hash);
    return g_cacheDir + "\\" + buf;
}

static void backgroundWorker() {
    while (true) {
        std::vector<WriteTask> toWrite;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return g_stopWorker.load() || !g_writeQueue.empty(); });

            if (g_stopWorker.load() && g_writeQueue.empty()) {
                break;
            }

            toWrite.swap(g_writeQueue);
        }

        for (const auto& task : toWrite) {
            FILE* f = std::fopen(task.path.c_str(), "wb");
            if (f) {
                std::fwrite(task.data.data(), 1, task.data.size(), f);
                std::fclose(f);
            }
        }
    }
}

static void WINAPI blobSet(const void* key, long keySize, const void* value, long valueSize) {
    if (!key || keySize <= 0 || !value || valueSize <= 0) return;

    auto path = keyToPath(key, keySize);
    std::vector<uint8_t> data((const uint8_t*)value, (const uint8_t*)value + valueSize);

    // 1. Immediate in-memory cache update (fast hit for blobGet)
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_ramCache[path] = data;
    }

    // 2. Async queue to background worker thread (zero main-thread disk I/O)
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_writeQueue.push_back({path, std::move(data)});
    }
    g_queueCv.notify_one();
}

static long WINAPI blobGet(const void* key, long keySize, void* value, long valueSize) {
    if (!key || keySize <= 0) return 0;

    auto path = keyToPath(key, keySize);

    // 1. Check in-memory RAM cache first
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_ramCache.find(path);
        if (it != g_ramCache.end()) {
            long size = (long)it->second.size();
            if (value && valueSize >= size) {
                std::memcpy(value, it->second.data(), (size_t)size);
            }
            return size;
        }
    }

    // 2. Disk fallback
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (size > 0) {
        std::vector<uint8_t> loadedData(size);
        std::fread(loadedData.data(), 1, (size_t)size, f);
        std::fclose(f);

        if (value && valueSize >= size) {
            std::memcpy(value, loadedData.data(), (size_t)size);
        }

        // Cache in memory for subsequent calls
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_ramCache[path] = std::move(loadedData);

        return size;
    }

    std::fclose(f);
    return 0;
}

} // anonymous namespace

namespace boost_shader_cache {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.shader_cache) return;

        g_cacheDir = cfg.shader_cache_dir;
        CreateDirectoryA(g_cacheDir.c_str(), nullptr);

        auto& a = angle::state();
        if (!a.egl) return;

        auto fn = (EglSetBlobCacheFunc)GetProcAddress(a.egl, "eglSetBlobCacheFuncsANDROID");
        if (!fn) {
            angle::log("shader_cache: eglSetBlobCacheFuncsANDROID not found in ANGLE");
            return;
        }

        // Start async disk writer thread
        g_stopWorker = false;
        g_workerThread = std::thread(backgroundWorker);

        fn(a.display, (void(*)(const void*, long, const void*, long))blobSet,
                       (long(*)(const void*, long, void*, long))blobGet);

        angle::log("shader_cache: active (async worker thread started), dir=%s", g_cacheDir.c_str());
    }

    void shutdown() {
        if (g_workerThread.joinable()) {
            {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_stopWorker = true;
            }
            g_queueCv.notify_all();
            g_workerThread.join();
        }
    }
}
