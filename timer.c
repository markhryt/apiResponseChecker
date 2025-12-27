#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

static double diff_ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0 +
        (b.tv_nsec - a.tv_nsec) / 1e6;
}

int main(int argc, char **argv) {
    if (argc != 2) return 1;

    const char *url = argv[1];
    const char *prefix = "http://";
    if (strncmp(url, prefix, 7) != 0) return 1;

    const char *p = url + 7;
    char host[256];
    char path[2048];
    int port = 80;

    const char *h = p;
    while (*p && *p != '/' && *p != ':') p++;
    size_t hlen = p - h;
    if (hlen == 0 || hlen >= sizeof(host)) return 1;
    memcpy(host, h, hlen);
    host[hlen] = 0;

    if (*p == ':') {
        p++;
        port = atoi(p);
        while (*p && *p != '/') p++;
    }

    if (*p) {
        strncpy(path, p, sizeof(path));
        path[sizeof(path) - 1] = 0;
    } else {
        strcpy(path, "/");
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    struct addrinfo *res, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    struct timespec dns_start, dns_end;
    clock_gettime(CLOCK_MONOTONIC, &dns_start);
    if (getaddrinfo(host, port_str, &hints, &res) != 0) return 1;
    clock_gettime(CLOCK_MONOTONIC, &dns_end);

    int fd = -1;
    struct timespec tcp_start, tcp_end;

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        clock_gettime(CLOCK_MONOTONIC, &tcp_start);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &tcp_end);
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return 1;

    fprintf(stderr, "DNS time: %.3f ms\n", diff_ms(dns_start, dns_end));
    fprintf(stderr, "TCP connect time: %.3f ms\n", diff_ms(tcp_start, tcp_end));

    char req[4096];
    int n = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host
            );
    send(fd, req, n, 0);

    char buf[4096];
    ssize_t r;
    while ((r = recv(fd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, r, stdout);
    }

    close(fd);
    return 0;
}

