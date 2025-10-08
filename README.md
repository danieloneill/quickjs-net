# quickjs-net
Basic low-level networking implementation for [QuickJS](https://bellard.org/quickjs/).

**This code currently builds against QuickJS-2025-09-13**

**This module does not currently work with [quickjs-ng](https://github.com/quickjs-ng/quickjs/) due to how that project implements os.setReadHandler.**

## Building
Edit Makefile and point QJSPATH to your quickjs root then just build with *make*.

Now you should be able to run the examples:
 * echo-server.js
 * httpclient.js
 * httpserver.js

## Documentation

 * socket(domain, type) -> fd
   * domain should be 'inet' or 'inet6'. ('unix' and 'local' are recognised but likely not useful)
   * type should be 'stream', 'dgram', or 'raw'
 * resolve(hostname, domain) -> [ { 'family':'inet', 'type':'dgram', 'ip':'208.113.236.64' } ]
   * hostname such as "oneill.app" or "battle.net"
   * domain should be one of 'inet' or 'inet6'
 * connect(fd, domain, address, port) -> true/false
   * fd as allocated from socket or accept
   * domain should be 'inet' or 'inet6'
   * address should be an ip as in a result object's ip trait as returned from resolve
   * port number in host byte order
 * bind(fd, domain, address, port) -> true/false
   * fd as allocated from socket or accept
   * domain should be 'inet' or 'inet6'
   * address should be an ip as in "::1" or "127.0.0.1", depending on domain
   * port number in host byte order
 * sync(fd) -> true/false
   * fd as allocated from socket or accept
   * same as man(2) syncfs
 * listen(fd, backlog) -> true/false
   * fd as allocated from socket or accept
   * same as man(3) listen almost exactly, but errors are Exceptions
 * accept(fd) -> { 'family':<'inet', 'inet6'>, 'ip':'1.2.3.4', 'port':54321, 'fd':5 }
   * fd is a socket as allocated by socket, bound with bind, and listening with listen
 * close(fd) -> true/false
 * shutdown(fd, how) -> true/false
   * where how is one of 'rd', 'wr', or 'rdwr' as a string

The *resolve* method is synchronous and will block execution while it's running. I may add an async method at some point, but for my purposes it's not an issue (because I don't even use it).

See examples, or read the source. It's not very long, and pretty plain C.

Don't be shy about making an issue or PR, and thanks for your interest.

Also check out my [quickjs-hash](https://github.com/danieloneill/quickjs-hash) and [quickjs-wolfssl](https://github.com/danieloneill/quickjs-wolfssl) modules.

