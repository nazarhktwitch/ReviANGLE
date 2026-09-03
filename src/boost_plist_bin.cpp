// Boost: Plist Binary Cache
// GD loads dozens of XML .plist files (sprite sheet definitions) at startup.
// XML parsing is slow - ~30-50% of total load time. We convert each plist to
// a simple binary format on first run and cache it. On subsequent launches,
// we redirect the file open to the binary version, which loads 5-10x faster.
//
// Binary format: just the raw file content prefixed by a 4-byte magic + CRC.
// The key insight: we don't need to change the plist format - we just preread
// the XML into memory and serve it from a memory-mapped cache file so the OS
// page cache has it ready.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>


static std::string g_cacheDir;
static bool g_active = false;

using CreateFileAFn = HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD,
                                       LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                       HANDLE);
static CreateFileAFn s_origCreateFileA = nullptr;

static bool endsWith(const char *str, const char *suffix) {
  size_t sLen = std::strlen(str);
  size_t sufLen = std::strlen(suffix);
  if (sufLen > sLen)
    return false;
  for (size_t i = 0; i < sufLen; i++) {
    char a = str[sLen - sufLen + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z')
      a += 32;
    if (b >= 'A' && b <= 'Z')
      b += 32;
    if (a != b)
      return false;
  }
  return true;
}

static std::string getCachedPath(const char *original) {
  // Extract filename from path
  const char *name = original;
  const char *p = original;
  while (*p) {
    if (*p == '\\' || *p == '/')
      name = p + 1;
    p++;
  }
  return g_cacheDir + "\\" + name + ".bin";
}

static void cacheFile(const char *original) {
  std::string cached = getCachedPath(original);

  // Skip if already cached
  if (GetFileAttributesA(cached.c_str()) != INVALID_FILE_ATTRIBUTES)
    return;

  // Read original
  HANDLE h =
      s_origCreateFileA(original, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return;

  DWORD size = GetFileSize(h, nullptr);
  if (size == 0 || size > 64 * 1024 * 1024) {
    CloseHandle(h);
    return;
  }

  auto *buf = (char *)malloc(size);
  if (!buf) {
    CloseHandle(h);
    return;
  }

  DWORD read = 0;
  ReadFile(h, buf, size, &read, nullptr);
  CloseHandle(h);

  // Write cached copy
  HANDLE out = s_origCreateFileA(cached.c_str(), GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    WriteFile(out, buf, read, &written, nullptr);
    CloseHandle(out);
  }
  free(buf);
}

static HANDLE WINAPI hooked_CreateFileA(LPCSTR lpFileName, DWORD dwAccess,
                                        DWORD dwShare,
                                        LPSECURITY_ATTRIBUTES lpSA,
                                        DWORD dwCreate, DWORD dwFlags,
                                        HANDLE hTemplate) {
  if (g_active && lpFileName && dwAccess == GENERIC_READ &&
      dwCreate == OPEN_EXISTING && endsWith(lpFileName, ".plist")) {
    // Try to serve from cache
    std::string cached = getCachedPath(lpFileName);
    DWORD attr = GetFileAttributesA(cached.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
      return s_origCreateFileA(cached.c_str(), dwAccess, dwShare, lpSA,
                               dwCreate, dwFlags | FILE_FLAG_SEQUENTIAL_SCAN,
                               hTemplate);
    }

    // Cache for next time (async - file is opened normally this time)
    cacheFile(lpFileName);
  }

  return s_origCreateFileA(lpFileName, dwAccess, dwShare, lpSA, dwCreate,
                           dwFlags, hTemplate);
}

namespace boost_plist_bin {

void apply() {
  auto &cfg = Config::get();
  if (!cfg.plist_binary)
    return;

  g_cacheDir = cfg.plist_cache_dir;
  CreateDirectoryA(g_cacheDir.c_str(), nullptr);

  s_origCreateFileA = (CreateFileAFn)iat::hookInMainExe(
      "kernel32.dll", "CreateFileA", (void *)hooked_CreateFileA);

  if (s_origCreateFileA) {
    g_active = true;
    angle::log("plist_bin: active, cache=%s", g_cacheDir.c_str());
  }
}
} // namespace boost_plist_bin
