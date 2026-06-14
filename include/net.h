#ifndef NET_H
#define NET_H

/* Capa de abstraccion de sockets: oculta las diferencias entre Winsock (Windows)
   y los sockets POSIX (Linux) tras una sola interfaz. El resto del codigo no
   debe tener #ifdef de red: todo vive aqui. */

#include <stdint.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define SOCK_INVALID INVALID_SOCKET
#else
  typedef int sock_t;
  #define SOCK_INVALID (-1)
#endif

int    net_init(void);                 /* WSAStartup en Windows; no-op en Linux. 0 = ok */
void   net_cleanup(void);

sock_t net_listen(uint16_t port);      /* crea socket de escucha (TCP). SOCK_INVALID si falla */
sock_t net_accept(sock_t listen_sock); /* acepta un cliente */
sock_t net_connect(const char *host, uint16_t port); /* conecta a host:port */

int    net_send_all(sock_t s, const void *buf, int len); /* 0 = ok, -1 = error */
int    net_recv_all(sock_t s, void *buf, int len);       /* 1 = ok, 0 = cerrado, -1 = error */

void   net_close(sock_t s);

#endif /* NET_H */
