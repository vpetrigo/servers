#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <algorithm>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
  int maxfd = 0;
  int master_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  struct sockaddr_in sin;
  int status = inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);
  fd_set dlist;

  sin.sin_family = AF_INET;
  sin.sin_port = htons(12345);
  maxfd = std::max(maxfd, master_socket);

  if (status != 1) {
    throw std::runtime_error{"cannot init IP address"};
  }

  status = bind(master_socket, reinterpret_cast<struct sockaddr *>(&sin),
                sizeof sin);

  if (status) {
    int errv = errno;

    throw std::runtime_error{strerror(errv)};
  }

  FD_ZERO(&dlist);
  FD_SET(master_socket, &dlist);

  status = listen(master_socket, SOMAXCONN);

  if (status) {
    int errv = errno;

    throw std::runtime_error{strerror(errv)};
  }

  while (true) {
    select(maxfd + 1, &dlist, nullptr, nullptr, nullptr);

    for (std::size_t i = master_socket; i < maxfd + 1; ++i) {
      if (FD_ISSET(i, &dlist) && i == master_socket) {
        std::cout << "Master action" << std::endl;
        struct sockaddr_in client_addr = {0};
        int client_fd;
        unsigned int len = sizeof client_addr;

        client_fd =
            accept(master_socket,
                   reinterpret_cast<struct sockaddr *>(&client_addr), &len);
        int flags = fcntl(client_fd, F_GETFL, nullptr);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        FD_SET(client_fd, &dlist);
        maxfd = std::max(maxfd, client_fd);
        std::cout << "maxfd: " << maxfd << std::endl;
      } else if (FD_ISSET(i, &dlist) && i != master_socket) {
        std::cout << "Client action: " << i << std::endl;
        char buf[1024];
        int len = read(i, buf, 1024);
        std::cout << len << std::endl;
        if (len >= 0) {
          std::cout << "Read: " << buf << std::endl;
          std::cout << "Bytes: " << len << std::endl;
          write(i, buf, len);
        } else {
          if (errno != EAGAIN) {
            shutdown(i, SHUT_RDWR);
            close(i);
            FD_CLR(i, &dlist);

            if (i == maxfd) {
              --maxfd;
            }
          }
        }
      }
    }
  }

  return 0;
}
