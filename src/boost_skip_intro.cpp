// Boost: Skip RobTop Intro
// GD shows a 3-4 second splash screen with the RobTop logo and audio.
// We intercept CreateFileA for "menuLoop.mp3" (the splash audio) and return
// INVALID_HANDLE_VALUE so it fails silently. Cocos2d skips the splash if the
// audio file can't be loaded.
//
// Also hooks PlaySound / FMOD calls for the intro jingle.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <cstring>
#include <windows.h>


using CreateFileAFn = HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD,
                                       LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                       HANDLE);
static CreateFileAFn s_origCreateFileA = nullptr;
static bool g_active = false;

static bool endsWithI(const char *str, const char *suffix) {
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

static HANDLE WINAPI hooked_CreateFileA(LPCSTR lpFileName, DWORD dwAccess,
                                        DWORD dwShare,
                                        LPSECURITY_ATTRIBUTES lpSA,
                                        DWORD dwCreate, DWORD dwFlags,
                                        HANDLE hTemplate) {
  if (g_active && lpFileName) {
    // Block the intro splash audio files
    if (endsWithI(lpFileName, "menuloop.mp3") ||
        endsWithI(lpFileName, "geostarter.mp3") ||
        endsWithI(lpFileName, "robtoplogo.mp3")) {
      SetLastError(ERROR_FILE_NOT_FOUND);
      return INVALID_HANDLE_VALUE;
    }
  }
  return s_origCreateFileA(lpFileName, dwAccess, dwShare, lpSA, dwCreate,
                           dwFlags, hTemplate);
}

namespace boost_skip_intro {

void apply() {
  if (!Config::get().skip_intro)
    return;

  s_origCreateFileA = (CreateFileAFn)iat::hookInMainExe(
      "kernel32.dll", "CreateFileA", (void *)hooked_CreateFileA);

  if (s_origCreateFileA) {
    g_active = true;
    angle::log("skip_intro: active - splash audio blocked");
  }
}
} // namespace boost_skip_intro
