# quickjs-net
Basic low-level networking implementation for [QuickJS](https://bellard.org/quickjs/).

**This code currently builds against QuickJS-2025-09-13**

**This module does not currently work with [quickjs-ng](https://github.com/quickjs-ng/quickjs/) due to how that project implements os.setReadHandler.**

## Building
Edit Makefile and point QJSPATH to your quickjs root then just build with *make*.

Now you should be able to run the examples:
 * echo-server.js
  * Run `qjs echo-server.js`, then connect to it via `telnet ::1 54321`.

 * httpclient.js
  * Run `qjs httpclient.js` to download an image file to *out.jpg*.

 * httpserver.js
  * Run `qjs httpserver.js` and then, on the same PC, point a browser to *http://[::1]:8081*

## Documentation

### Constants

Many constants are exposed on the module root such as:
```
import * as net from "net.so";

let fd = net.socket(net.AF_INET6, net.SOCK_STREAM);
```

These constants are:
```
 * SOL_SOCKET
 * SO_ERROR
 * AF_INET
 * AF_INET6
 * AF_LOCAL
 * AF_UNIX
 * AF_UNSPEC
 * SHUT_RDWR
 * SHUT_RD
 * SHUT_WR
 * SOCK_STREAM
 * SOCK_DGRAM
 * SOCK_RAW
 * EAGAIN
 * EWOULDBLOCK
 * EINPROGRESS
```

### Methods

All methods are on the globally allocated module object and include:

 * accept(handle) -> { 'family':(same as domain in `socket`), 'ip':'1.2.3.4', 'port':54321, 'fd':5 }
   * handle is allocated by socket, bound with bind, and listening with listen
   * ip and port will be absent if family is of type AF_UNIX
 * bind(fd, address, port) -> true/false
   * handle as allocated from socket
   * address should be an ip as in "::1" or "127.0.0.1", or a unix path as in "/tmp/mysocket", depending on domain
   * port number in host byte order, and should not be passed for unix sockets
 * connect(handle, address, port) -> true/false
   * handle as allocated from socket
   * address should be an ip as in a result object's ip trait as returned from resolve, or a unix path such as "@/tmp/.X11-unix/X0" if it's a UNIX socket
   * port number in host byte order, and should not be passed for unix sockets
 * familyname(domain) -> string
   * domain should be net.AF_INET, AF_INET6, AF_UNSPEC, or AF_UNIX
 * listen(handle, backlog) -> true/false
   * handle as allocated from socket
   * same as man(3) listen almost exactly, but errors are Exceptions
 * resolve(hostname, domain) -> [ { 'family':net.AF_INET6, 'type':net.SOCK_DGRAM, 'ip':'208.113.236.64' } ]
   * hostname such as "oneill.app" or "battle.net"
   * domain should be either net.AF_INET or net.AF_INET6
   * **See notes below regarding this method!**
 * socket(domain, type) -> handle
   * domain should be net.AF_INET, AF_INET6, or AF_UNIX
   * type should be net.SOCK_STREAM, SOCK_DGRAM, or SOCK_RAW
 * socktypename(type) -> string
   * type should be net.SOCK_STREAM, SOCK_DGRAM, or SOCK_RAW
 * shutdown(fd, how) -> true/false
   * where how is one of net.SHUT_RD, SHUT_WR, or SHUT_RDWR
 * sync(handle) -> true/false
   * handle as allocated from socket or accept
   * same as man(2) syncfs
 
`handle` is like: `{ "fd":123, "family":10, "type":3 }` where "fd" can be used with std/os methods such as os.setReadHandler, "family" is net.AF_UNIX, net.AF_INET, etc, and "type" is net.SOCK_STREAM, net.SOCK_DGRAM, etc.

The *resolve* method is synchronous and will block execution while it's running. I may add an async method at some point, but for my purposes it's not an issue (because I don't even use it).

See examples, or read the source. It's not very long, and pretty plain C.

Don't be shy about making an issue or PR, and thanks for your interest.

Also check out my [quickjs-hash](https://github.com/danieloneill/quickjs-hash), [quickjs-dbi](https://github.com/danieloneill/quickjs-dbi) and [quickjs-wolfssl](https://github.com/danieloneill/quickjs-wolfssl) modules.

