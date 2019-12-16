#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_PREVIEW_LEN 40      // always previews/prints that amount, regardless of whether the buffer holds that much or not

/* overwrite socket() */
typedef int (*real_socket_t)(int, int, int);
int real_socket(int domain, int type, int protocol)
{
    return ((real_socket_t) dlsym(RTLD_NEXT, "socket")) (domain, type, protocol);
}
int socket(int domain, int type, int protocol)
{
    printf("[-] Opened a new socket (domain: %d, type: %d, protocol: %d)\n",
           domain, type, protocol);
    return real_socket(domain, type, protocol);
}


/* overwrite recv() */
typedef ssize_t(*real_recv_t) (int, void *, size_t, int);
ssize_t real_recv(int sockfd, void *buf, size_t len, int flags)
{
    return ((real_recv_t) dlsym(RTLD_NEXT, "recv")) (sockfd, buf, len, flags);
}
ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    printf("[-] Read using `recv` from socket (fd: %d)\n", sockfd);
    ssize_t ret = real_recv(sockfd, buf, len, flags);
    fwrite(buf, sizeof(char), BUF_PREVIEW_LEN, stdout);
    puts("[...]\n");
    return ret;
}


/* overwrite send() */

// checks, if the corresponding socket fd is (probably) for HTTP traffic
int is_http(int sockfd)
{
    int type;
    socklen_t olen = sizeof(type);
    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &type, &olen) == -1) {
        perror("getsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    socklen_t nlen = sizeof(addr);
    if (getpeername(sockfd, (struct sockaddr *)&addr, &nlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    int port = ntohs(addr.sin_port);

    printf("    socket %d has type %d and is bound to port the remote port %d\n", sockfd, type, port);

    return (type == SOCK_STREAM) && (port == 80);
}

void overwrite_header(char *buf)
{
    char needle[] = "User-Agent: ";
    char *pos = strstr(buf, needle) + strlen(needle) - 1;
    while (*++pos != '\r') {
        *pos = 'T';
    }
}

typedef ssize_t(*real_send_t) (int, const void *, size_t, int);
ssize_t real_send(int sockfd, const void *buf, size_t len, int flags)
{
    return ((real_send_t) dlsym(RTLD_NEXT, "send")) (sockfd, buf, len, flags);
}
ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    printf("[-] Write using `send` from socket (fd: %d)\n", sockfd);

    if (is_http(sockfd)) {
        puts("[+] detected HTTP traffic: overwriting the user agent in the HTTP header...\n");
        overwrite_header((char *)buf);
    }

    ssize_t ret = real_send(sockfd, buf, len, flags);
    fwrite(buf, sizeof(char), ret, stdout);
    return ret;
}


/* overwrite close () */
// checks, if a file descriptor is a socket
int is_socket(int fd)
{
    struct stat info;
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    return (info.st_mode & S_IFMT) == S_IFSOCK;
}

typedef int (*real_close_t)(int);
int real_close(int fd)
{
    return ((real_close_t) dlsym(RTLD_NEXT, "close")) (fd);
}
int close(int fd)
{
    if (is_socket(fd)) {
        printf("[-] Closed socket (fd: %d)\n", fd);
    }
    return real_close(fd);
}
