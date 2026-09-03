// Boost: RAM-backed file cache
// Copies Resources/ directory into a temp folder at launch, then hooks file
// opens to redirect to the cached copy. Windows caches temp files in RAM,
// so subsequent reads are instant.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <cstdio>
#include <string>
#include <windows.h>


static std::string g_cacheDir;
static bool g_active = false;

using CreateFileAFn = HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD,
                                       LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                       HANDLE);
static CreateFileAFn s_origCreateFileA = nullptr;

static bool startsWith(const char *str, const char *prefix) {
  while (*prefix) {
    if (*str != *prefix)
      return false;
    str++;
    prefix++;
  }
  return true;
}

static HANDLE WINAPI hooked_CreateFileA(LPCSTR lpFileName, DWORD dwAccess,
                                        DWORD dwShare,
                                        LPSECURITY_ATTRIBUTES lpSA,
                                        DWORD dwCreate, DWORD dwFlags,
                                        HANDLE hTemplate) {
  if (g_active && dwAccess == GENERIC_READ && dwCreate == OPEN_EXISTING) {
    // check if path starts with Resources\\ or Resources/
    if (startsWith(lpFileName, "Resources\\") ||
        startsWith(lpFileName, "Resources/")) {
      std::string cached =
          g_cacheDir + "\\" + (lpFileName + 10); // skip "Resources/"
      DWORD attr = GetFileAttributesA(cached.c_str());
      if (attr != INVALID_FILE_ATTRIBUTES) {
        return s_origCreateFileA(cached.c_str(), dwAccess, dwShare, lpSA,
                                 dwCreate, dwFlags | FILE_FLAG_SEQUENTIAL_SCAN,
                                 hTemplate);
      }
    }
  }
  return s_origCreateFileA(lpFileName, dwAccess, dwShare, lpSA, dwCreate,
                           dwFlags, hTemplate);
}

static void copyResources() {
  WIN32_FIND_DATAA fd;
  HANDLE fh = FindFirstFileA("Resources\\*", &fd);
  if (fh == INVALID_HANDLE_VALUE)
    return;

  int count = 0;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;
    std::string src = std::string("Resources\\") + fd.cFileName;
    std::string dst = g_cacheDir + "\\" + fd.cFileName;
    if (CopyFileA(src.c_str(), dst.c_str(), TRUE))
      count++;
  } while (FindNextFileA(fh, &fd));
  FindClose(fh);

  angle::log("ramdisk: copied %d files to %s", count, g_cacheDir.c_str());
}

namespace boost_ramdisk {

void apply() {
  auto &cfg = Config::get();
  if (!cfg.ramdisk_cache)
    return;

  if (cfg.ramdisk_path.empty()) {
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    g_cacheDir = std::string(temp) + "gd_ramcache";
  } else {
    g_cacheDir = cfg.ramdisk_path;
  }

  CreateDirectoryA(g_cacheDir.c_str(), nullptr);
  copyResources();

  s_origCreateFileA = (CreateFileAFn)iat::hookInMainExe(
      "kernel32.dll", "CreateFileA", (void *)hooked_CreateFileA);

  if (s_origCreateFileA) {
    g_active = true;
    angle::log("ramdisk: active, cache=%s", g_cacheDir.c_str());
  }
}

void shutdown() {
  // optionally clean up temp files
  if (!g_cacheDir.empty()) {
    // leave them - Windows cleans %TEMP% periodically
  }
}
} // namespace boost_ramdisk
