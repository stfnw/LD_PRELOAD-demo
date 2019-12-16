LD_PRELOAD demo
===============

LD_PRELOAD instructs the dynamic linker/loader to preload user-specified shared objects before all others.
This can be used to inject/override exported symbols which are provided in dynamically linked libraries.
Good explanations can be found [here](http://www.goldsborough.me/c/low-level/kernel/2016/08/29/16-48-53-the_-ld_preload-_trick/) as well as under `ld.so(8)` and `dlsym(3)`.

In this demo we're going to override some of the network related syscalls or --
to be more precise -- their corresponding wrappers provided by glibc which are
listed  in `socket(7)`.
`override.c` wraps and logs invocations of `socket(2)`, `recv(2)`, `send(2)` and
`close(2)` to stdout.
Furthermore -- just as demonstration of capabilities -- `send(2)` was modified
to detect HTTP traffic and replace the User-Agent in each request with `T`s
before the data is actually send out to the network:

```c
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
```

For the demo invocation included in the Makefile we are going to use curl.
We can verify that glibc is indeed dynamically linked for curl and that
therefore our `LD_PRELOAD` will affect the curl invocation by executing `ldd /usr/bin/curl`:

```shell
$ ldd /usr/bin/curl
# [...]
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f71b6258000)
# [...]
```


Example invocation
------------------

    make

...compiles the program into a shared library...

    cc -g -Wall -Werror -std=c99 -fPIC   -c -o override.o override.c
    cc -shared -ldl override.o -o override.so

...and then executes some demo calls which generate network traffic to wikipedia.org.
First with HTTPS -- the function calls are just logged:
Here `curl --verbose` shows the sent header, including `User-Agent: curl/7.64.0`.

    [+] Trying out HTTPS traffic
    LD_PRELOAD=./override.so curl --verbose https://wikipedia.org
    [...]
    > GET / HTTP/2
    > Host: wikipedia.org
    > User-Agent: curl/7.64.0
    > Accept: */*
    [...]
    [-] Opened a new socket (domain: 10, type: 2, protocol: 0)
    [-] Closed socket (fd: 3)
    [-] Opened a new socket (domain: 2, type: 526338, protocol: 0)
    [-] Opened a new socket (domain: 10, type: 1, protocol: 6)
    [...]
    [-] Closed socket (fd: 3)

...and secondly with HTTP -- where the user agent is replaced:

    [+] Trying out HTTP traffic
    LD_PRELOAD=./override.so curl --silent http://wikipedia.org
    [...]
    > GET / HTTP/1.1
    > Host: wikipedia.org
    > User-Agent: TTTTTTTTTTT
    > Accept: */*
    [...]
    [-] Opened a new socket (domain: 10, type: 2, protocol: 0)
    [-] Closed socket (fd: 3)
    [-] Opened a new socket (domain: 2, type: 526338, protocol: 0)
    [-] Opened a new socket (domain: 10, type: 1, protocol: 6)
    [-] Write using `send` from socket (fd: 3)
        socket 3 has type 1 and is bound to port the remote port 80
    [+] detected HTTP traffic: overwriting the user agent in the HTTP header...

    GET / HTTP/1.1
    Host: wikipedia.org
    User-Agent: TTTTTTTTTTT
    Accept: */*

    [-] Read using `recv` from socket (fd: 3)
    HTTP/1.1 301 TLS Redirect
    Date: Fri, 06[...]

    [-] Closed socket (fd: 3)
