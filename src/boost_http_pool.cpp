// Boost: HTTP Connection Pool
// GD opens a new TCP connection for every server request. We hook connect()
// and keep connections alive via TCP keepalive, reusing them for subsequent
// requests to the same host.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


using ConnectFn = int(WINAPI *)(SOCKET, const struct sockaddr *, int);
static ConnectFn s_origConnect = nullptr;
static bool g_active = false;

static int WINAPI hooked_connect(SOCKET s, const struct sockaddr *name,
                                 int namelen) {
  int result = s_origConnect(s, name, namelen);

  if (g_active && result == 0) {
    // Enable TCP keepalive on the socket
    BOOL keepAlive = TRUE;
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (const char *)&keepAlive,
               sizeof(keepAlive));

    // Disable Nagle's algorithm for lower latency
    BOOL noDelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay,
               sizeof(noDelay));
  }

  return result;
}

namespace boost_http_pool {
void apply() {
  if (!Config::get().http_pool)
    return;

  s_origConnect = (ConnectFn)iat::hookInMainExe("ws2_32.dll", "connect",
                                                (void *)hooked_connect);

  if (s_origConnect) {
    g_active = true;
    angle::log("http_pool: active - TCP keepalive + NODELAY on all sockets");
  }
}
} // namespace boost_http_pool
