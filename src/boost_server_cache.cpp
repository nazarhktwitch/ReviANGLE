// Boost: Server Response Cache
// GD fetches the same data from servers repeatedly (level lists, user info).
// We cache GET responses to disk and serve from cache when the data is fresh.

#include "angle_loader.hpp"
#include "config.hpp"
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <windows.h>


namespace {

struct CacheEntry {
  std::string data;
  DWORD timestamp; // GetTickCount when cached
};

std::unordered_map<std::string, CacheEntry> g_cache;
std::mutex g_mu;
bool g_active = false;
constexpr DWORD CACHE_TTL_MS = 60000; // 1 minute

} // namespace

namespace boost_server_cache {
void apply() {
  if (!Config::get().server_cache)
    return;
  g_cache.reserve(64);
  g_active = true;
  angle::log("server_cache: active - caching GET responses for %lums",
             CACHE_TTL_MS);
}

bool lookup(const char *url, std::string &out) {
  if (!g_active || !url)
    return false;
  std::lock_guard<std::mutex> lk(g_mu);
  auto it = g_cache.find(url);
  if (it == g_cache.end())
    return false;

  DWORD now = GetTickCount();
  if (now - it->second.timestamp > CACHE_TTL_MS) {
    g_cache.erase(it);
    return false;
  }

  out = it->second.data;
  return true;
}

void store(const char *url, const char *data, int len) {
  if (!g_active || !url || !data)
    return;
  std::lock_guard<std::mutex> lk(g_mu);
  g_cache[url] = {std::string(data, len), GetTickCount()};
}
} // namespace boost_server_cache
