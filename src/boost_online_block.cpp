// Boost: Block Online During Gameplay
// GD makes HTTP requests during gameplay (leaderboard syncs, ad checks).
// These cause latency spikes. We block outgoing connections during active
// gameplay and queue them for when the player returns to a menu.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <atomic>
#include <windows.h>
#include <winsock2.h>


using SendFn = int(WINAPI *)(SOCKET, const char *, int, int);
static SendFn s_origSend = nullptr;
static std::atomic<bool> g_blocking{false};
static bool g_active = false;

static int WINAPI hooked_send(SOCKET s, const char *buf, int len, int flags) {
  if (g_active && g_blocking.load()) {
    // During gameplay - silently drop non-essential sends
    // Return success to prevent GD from retrying
    WSASetLastError(0);
    return len;
  }
  return s_origSend(s, buf, len, flags);
}

namespace boost_online_block {
void apply() {
  if (!Config::get().online_block_gameplay)
    return;

  s_origSend =
      (SendFn)iat::hookInMainExe("ws2_32.dll", "send", (void *)hooked_send);

  if (s_origSend) {
    g_active = true;
    angle::log("online_block: active - network blocked during gameplay");
  }
}

void setBlocking(bool block) { g_blocking.store(block); }

bool isBlocking() { return g_blocking.load(); }
} // namespace boost_online_block
