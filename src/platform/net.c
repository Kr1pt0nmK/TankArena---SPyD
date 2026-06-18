#include "net.h"
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#ifdef __linux__
  #define SEND_FLAGS MSG_NOSIGNAL   /* evita SIGPIPE si el peer cierra */
#else
  #define SEND_FLAGS 0
#endif

int net_init(void)
{
#ifdef _WIN32
    WSADATA w;
    return WSAStartup(MAKEWORD(2, 2), &w) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void net_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void net_close(sock_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static void set_nodelay(sock_t s)
{
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
}

sock_t net_listen(uint16_t port)
{
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALID)
        return SOCK_INVALID;

    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(port);

    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) { net_close(s); return SOCK_INVALID; }
    if (listen(s, 8) != 0)                              { net_close(s); return SOCK_INVALID; }
    return s;
}

sock_t net_accept(sock_t listen_sock)
{
    sock_t c = accept(listen_sock, NULL, NULL);
    if (c != SOCK_INVALID)
        set_nodelay(c);
    return c;
}

sock_t net_connect(const char *host, uint16_t port)
{
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

    struct addrinfo hints, *res = NULL, *it;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, portstr, &hints, &res) != 0)
        return SOCK_INVALID;

    sock_t s = SOCK_INVALID;
    for (it = res; it; it = it->ai_next) {
        s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (s == SOCK_INVALID)
            continue;
        if (connect(s, it->ai_addr, (int)it->ai_addrlen) == 0) {
            set_nodelay(s);
            break;
        }
        net_close(s);
        s = SOCK_INVALID;
    }
    freeaddrinfo(res);
    return s;
}

int net_send_all(sock_t s, const void *buf, int len)
{
    const char *p = (const char *)buf;
    int left = len;
    while (left > 0) {
        int n = (int)send(s, p, left, SEND_FLAGS);
        if (n <= 0)
            return -1;
        p += n;
        left -= n;
    }
    return 0;
}

int net_recv_all(sock_t s, void *buf, int len)
{
    char *p = (char *)buf;
    int left = len;
    while (left > 0) {
        int n = (int)recv(s, p, left, 0);
        if (n == 0) return 0;   /* peer cerro */
        if (n < 0)  return -1;  /* error */
        p += n;
        left -= n;
    }
    return 1;
}

int net_peer_ip(sock_t s, char *out, int outlen)
{
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    if (getpeername(s, (struct sockaddr *)&a, &len) != 0)
        return -1;
    const char *p = inet_ntop(AF_INET, &a.sin_addr, out, outlen);
    return p ? 0 : -1;
}
