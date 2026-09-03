// Boost: Winsock Optimization
// Sets TCP_NODELAY and enlarged send/recv buffers on all sockets created
// by GD, reducing latency for server communication.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <windows.h>
#include <winsock2.h>


using SocketFn = SOCKET(WINAPI *)(int, int, int);
static SocketFn s_origSocket = nullptr;
static bool g_active = false;

static SOCKET WINAPI hooked_socket(int af, int type, int protocol) {
  SOCKET s = s_origSocket(af, type, protocol);

  if (g_active && s != INVALID_SOCKET && type == SOCK_STREAM) {
    // Disable Nagle
    BOOL noDelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay,
               sizeof(noDelay));

    // 64KB send/recv buffers
    int bufSize = 65536;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *)&bufSize,
               sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&bufSize,
               sizeof(bufSize));
  }

  return s;
}

namespace boost_winsock_opt {
void apply() {
  if (!Config::get().winsock_opt)
    return;

  s_origSocket = (SocketFn)iat::hookInMainExe("ws2_32.dll", "socket",
                                              (void *)hooked_socket);

  if (s_origSocket) {
    g_active = true;
    angle::log("winsock_opt: active - NODELAY + 64KB buffers");
  }
}
} // namespace boost_winsock_opt
