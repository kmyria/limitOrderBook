#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "incorrect usage\n";
        std::exit(1);
    }
    struct addrinfo hints {}, *res, *p;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    int status {};

    // service is null as we do not need to bind to port
    // as a server, first argument can be null since it is the name/ipaddr of
    // the server
    //
    // on the other hand, as a host, it may be null simply because we do not
    // have enough info, sometimes the port alone is enough to get some results.

    if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
        std::cerr << "gai error" << gai_strerror(status);
        std::exit(1);
    }

    int s {};
    void* addr;
    char buf[INET6_ADDRSTRLEN];

    // loop through results
    for (p = res; p != NULL; p = p->ai_next) {

        if (p->ai_family == AF_INET) {
            // ipv4-
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            // ipv6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(p->ai_family, addr, buf, sizeof buf);
        buf[INET6_ADDRSTRLEN - 1] = '\0';
        std::cout << buf << '\n';
    }

    freeaddrinfo(res);
    return 0;
}
