#include "quickjs-net.h"
#include "cutils.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

typedef struct {
    const char *name;
    int value;
} JSNETConstDef;

#define JSNETDEF(x) { #x, x }

static JSNETConstDef jsnetconsts[] = {
    JSNETDEF(SOL_SOCKET),
    JSNETDEF(SO_ERROR),
    JSNETDEF(AF_INET),
    JSNETDEF(AF_INET6),
    JSNETDEF(AF_LOCAL),
    JSNETDEF(AF_UNIX),
    JSNETDEF(AF_UNSPEC),
    JSNETDEF(SHUT_RDWR),
    JSNETDEF(SHUT_RD),
    JSNETDEF(SHUT_WR),
    JSNETDEF(SOCK_STREAM),
    JSNETDEF(SOCK_DGRAM),
    JSNETDEF(SOCK_RAW),
    JSNETDEF(EAGAIN),
    JSNETDEF(EWOULDBLOCK),
    JSNETDEF(EINPROGRESS),
};

static void js_set_error_object(JSContext *ctx, JSValue obj, int err)
{
    if (!JS_IsUndefined(obj)) {
        JS_SetPropertyStr(ctx, obj, "errno", JS_NewInt32(ctx, err));
    }
}

static int net_domain_to_int(const char *domain)
{
    if( strcasecmp(domain, "unix") == 0 )
        return AF_UNIX;
    else if( strcasecmp(domain, "local") == 0 )
        return AF_LOCAL;
    else if( strcasecmp(domain, "inet") == 0 )
        return AF_INET;
    else if( strcasecmp(domain, "inet6") == 0 )
        return AF_INET6;
    else if( strcasecmp(domain, "unspec") == 0 )
        return AF_UNSPEC;
    return -1;
}

static const char *net_int_to_domain(int domain)
{
    if( AF_UNIX == domain )
        return "unix";
    else if( AF_LOCAL == domain )
        return "local";
    else if( AF_INET == domain )
        return "inet";
    else if( AF_INET6 == domain )
        return "inet6";
    else if( AF_UNSPEC == domain )
        return "unspec";
    return "unknown";
}

static int net_socktype_to_int(const char *socktype)
{
    if( strcasecmp(socktype, "stream") == 0 )
        return SOCK_STREAM;
    else if( strcasecmp(socktype, "dgram") == 0 )
        return SOCK_DGRAM;
    else if( strcasecmp(socktype, "raw") == 0 )
        return SOCK_RAW;
    return -1;
}

static const char *net_int_to_socktype(int socktype)
{
    if( SOCK_STREAM == socktype )
        return "stream";
    else if( SOCK_DGRAM == socktype )
        return "dgram";
    else if( SOCK_RAW == socktype )
        return "raw";
    return "unknown";
}

static JSValue js_net_familyname(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    if( argc > 1 ) {
        JS_ThrowTypeError(ctx, "missing parameter");
        return JS_EXCEPTION;
    }

    int family;
    if( JS_ToInt32(ctx, &family, argv[0]) ) {
        JS_ThrowTypeError(ctx, "failed to parse parameter");
        return JS_EXCEPTION;
    }

    return JS_NewString(ctx, net_int_to_domain(family));
}

static JSValue js_net_socktypename(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    if( argc > 1 ) {
        JS_ThrowTypeError(ctx, "missing parameter");
        return JS_EXCEPTION;
    }

    int st;
    if( JS_ToInt32(ctx, &st, argv[0]) ) {
        JS_ThrowTypeError(ctx, "failed to parse parameter");
        return JS_EXCEPTION;
    }

    return JS_NewString(ctx, net_int_to_socktype(st));
}

static JSValue js_net_socket(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    const char *domain, *socktype = NULL;
    int fd, err, intdomain, inttype;

    if( argc < 2 ) {
        JS_ThrowTypeError(ctx, "insufficient parameters");
        return JS_EXCEPTION;
    }

    if( JS_ToInt32(ctx, &intdomain, argv[0]) ) {
        JS_ThrowTypeError(ctx, "invalid domain");
        return JS_EXCEPTION;
    }

    if( JS_ToInt32(ctx, &inttype, argv[1]) ) {
        JS_ThrowTypeError(ctx, "invalid type");
        return JS_EXCEPTION;
    }

    fd = socket(intdomain, inttype, 0);
    if (-1 == fd)
        err = errno;
    else
        err = 0;
    if (argc >= 3)
        js_set_error_object(ctx, argv[2], err);

    if (-1 == fd)
        return JS_NULL;
    return JS_NewInt64(ctx, fd);
}

static JSValue js_net_resolve(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    const char *node = NULL; //, *family = NULL;
    int s, err, resultIndex;
    JSValue resultList;

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    node = JS_ToCString(ctx, argv[0]);
    if (!node)
    {
        JS_ThrowTypeError(ctx, "node must be specified");
        goto fail;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    //hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    if( argc > 1 )
        JS_ToInt32(ctx, &hints.ai_family, argv[1]);

    s = getaddrinfo(node, NULL, &hints, &result);
    if( 0 != s )
    {
        JS_ThrowReferenceError(ctx, "%s", gai_strerror(s));
        goto fail;
    }

    resultList = JS_NewArray(ctx);
    resultIndex = 0;
    for( rp=result; rp != NULL; rp = rp->ai_next )
    {
        char outip[BUFSIZ];
        void *ptr = NULL;

        if( rp->ai_family == AF_INET )
            ptr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr;
        else if( rp->ai_family == AF_INET6 )
            ptr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr;

        const char *r = inet_ntop(rp->ai_family, ptr, outip, BUFSIZ-1);
        if( NULL == r )
        {
            printf("err: %s\n", strerror(errno));
            continue;
        }

        JSValue jsv_family = JS_NewInt32(ctx, rp->ai_family);
        JSValue jsv_type = JS_NewInt32(ctx, rp->ai_socktype);
        JSValue jsv_ip = JS_NewString(ctx, outip);

        JSValue ent = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ent, "family", jsv_family);
        JS_SetPropertyStr(ctx, ent, "type", jsv_type);
        JS_SetPropertyStr(ctx, ent, "ip", jsv_ip);

        JS_SetPropertyUint32(ctx, resultList, resultIndex++, ent);
    }
    freeaddrinfo(result);

    JS_FreeCString(ctx, node);
    return resultList;
fail:
    JS_FreeCString(ctx, node);
    return JS_EXCEPTION;
}

static JSValue js_net_connect(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    unsigned char addrbuf[sizeof(struct in6_addr)];
    const char *addr=NULL;
    int64_t fd, port;
    int int_family;

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    struct sockaddr_un sau;
    struct sockaddr *sa;
    int socklen;

    if( argc < 3 ) {
        JS_ThrowTypeError(ctx, "expect fd, family, and address at least");
        goto fail;
    }

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    if( JS_ToInt32(ctx, &int_family, argv[1]) ) {
        JS_ThrowTypeError(ctx, "invalid family");
        goto fail;
    }

    addr = JS_ToCString(ctx, argv[2]);
    if (!addr)
    {
        JS_ThrowTypeError(ctx, "addr must be specified");
        goto fail;
    }

    addrbuf[0] = '\0';
    if( AF_UNIX != int_family ) {
        if( argc < 4 ) {
            JS_ThrowTypeError(ctx, "port must be specified");
            goto fail;
        }

        if( JS_ToInt64(ctx, &port, argv[3]) )
            return JS_EXCEPTION;

        if( -1 == inet_pton(int_family, addr, &addrbuf) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }

    if( AF_INET == int_family )
    {
        memset( &sa4, 0, sizeof(sa4) );
        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(port);
        memcpy( &sa4.sin_addr, &addrbuf, sizeof(struct in_addr) );

        sa = (struct sockaddr *)&sa4;
        socklen = sizeof(struct sockaddr_in);
    }
    else if( AF_INET6 == int_family )
    {
        memset( &sa6, 0, sizeof(sa6) );
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(port);
        memcpy( &sa6.sin6_addr, &addrbuf, sizeof(struct in6_addr) );

        sa = (struct sockaddr *)&sa6;
        socklen = sizeof(struct sockaddr_in6);
    }
    else if( AF_UNIX == int_family )
    {
        memset( &sau, 0, sizeof(sau) );
        sau.sun_family = AF_UNIX;
        strncpy(sau.sun_path, addr, sizeof(sau.sun_path)-1);

        sa = (struct sockaddr *)&sau;
        socklen = offsetof(struct sockaddr_un, sun_path) + strlen(sau.sun_path) + 1;

        // Handle '@'. 107->106 char limit bug here maybe, but... meh.
        if( '@' == addr[0] && socklen > 0 ) {
            sau.sun_path[0] = '\0';
            socklen--;
        }
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        sau.sun_len = socklen;
#endif
    }
    else
    {
        JS_ThrowTypeError(ctx, "unsupported family");
        goto fail;
    }

    if( -1 == connect(fd, sa, socklen) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        goto fail;
    }

    if( addr )
        JS_FreeCString(ctx, addr);

    return JS_TRUE;
fail:
    if( addr )
        JS_FreeCString(ctx, addr);
    return JS_FALSE;
}

static JSValue js_net_bind(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    unsigned char addrbuf[sizeof(struct in6_addr)];
    const char *addr=NULL;
    int64_t fd, port;
    int int_family, one=1;

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    struct sockaddr_un sau;
    struct sockaddr *sa;
    int socklen;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    if( JS_ToInt32(ctx, &int_family, argv[1]) )
        return JS_EXCEPTION;

    addr = JS_ToCString(ctx, argv[2]);
    if (!addr)
    {
        JS_ThrowTypeError(ctx, "addr must be specified");
        goto fail;
    }

    addrbuf[0] = '\0';
    if( AF_UNIX != int_family ) {
        if( argc < 4 ) {
            JS_ThrowTypeError(ctx, "port must be specified");
            goto fail;
        }

        if( JS_ToInt64(ctx, &port, argv[3]) )
            return JS_EXCEPTION;

        if( -1 == inet_pton(int_family, addr, &addrbuf) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(int));

    if( AF_INET == int_family )
    {
        memset( &sa4, 0, sizeof(sa4) );
        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(port);
        memcpy( &sa4.sin_addr, &addrbuf, sizeof(struct in_addr) );

        sa = (struct sockaddr *)&sa4;
        socklen = sizeof(struct sockaddr_in);
    }
    else if( AF_INET6 == int_family )
    {
        memset( &sa6, 0, sizeof(sa6) );
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(port);
        memcpy( &sa6.sin6_addr, &addrbuf, sizeof(struct in6_addr) );

        sa = (struct sockaddr *)&sa6;
        socklen = sizeof(struct sockaddr_in6);
    }
    else if( AF_UNIX == int_family )
    {
        memset( &sau, 0, sizeof(sau) );
        sau.sun_family = AF_UNIX;
        strncpy(sau.sun_path, addr, sizeof(sau.sun_path)-1);

        sa = (struct sockaddr *)&sau;
        socklen = offsetof(struct sockaddr_un, sun_path) + strlen(sau.sun_path) + 1;

        // Handle '@'. 107->106 char limit bug here maybe, but... meh.
        if( '@' == addr[0] && socklen > 0 ) {
            sau.sun_path[0] = '\0';
            socklen--;
        }
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        sau.sun_len = socklen;
#endif
    } else {
        // TODO: Support AF_UNIX too
        JS_ThrowTypeError(ctx, "unsupported family");
        goto fail;
    }

    if( -1 == bind(fd, sa, socklen) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        goto fail;
    }

    JS_FreeCString(ctx, addr);
    return JS_TRUE;
fail:
    JS_FreeCString(ctx, addr);
    return JS_FALSE;
}

static JSValue js_net_sync(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    int64_t fd = 0;
    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    return 0 == syncfs(fd) ? JS_TRUE : JS_FALSE;
}

static JSValue js_net_listen(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    int64_t fd, backlog;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    if( JS_ToInt64(ctx, &backlog, argv[1]) )
        return JS_EXCEPTION;

    int s = listen(fd, backlog);
    if( -1 == s )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    return JS_TRUE;
}

static JSValue js_net_accept(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    struct sockaddr_storage sockaddr;
    socklen_t socklen = sizeof(sockaddr);
    int64_t infd, outfd, port=0;
    char outip[BUFSIZ];
    void *ptr = NULL;

    if( JS_ToInt64(ctx, &infd, argv[0]) )
        return JS_EXCEPTION;

    outfd = accept(infd, (struct sockaddr *)&sockaddr, &socklen);
    if( -1 == outfd )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    JSValue ent = JS_NewObject(ctx);
    JSValue jsv_family = JS_NewInt32(ctx, sockaddr.ss_family);
    JSValue jsv_fd = JS_NewInt64(ctx, outfd);
    JS_SetPropertyStr(ctx, ent, "family", jsv_family);
    JS_SetPropertyStr(ctx, ent, "fd", jsv_fd);

    if( sockaddr.ss_family == AF_INET )
    {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sockaddr;
        ptr = &s4->sin_addr;
        port = ntohs( s4->sin_port );
    }
    else if( sockaddr.ss_family == AF_INET6 )
    {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sockaddr;
        ptr = &s6->sin6_addr;
        port = ntohs( s6->sin6_port );
    }

    if( ptr ) {
        const char *r = inet_ntop(sockaddr.ss_family, ptr, outip, BUFSIZ-1);
        if( NULL == r )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            JS_FreeValue(ctx, jsv_fd);
            JS_FreeValue(ctx, jsv_family);
            JS_FreeValue(ctx, ent);
            return JS_UNDEFINED;
        }

        JSValue jsv_ip = JS_NewString(ctx, outip);
        JS_SetPropertyStr(ctx, ent, "ip", jsv_ip);
    }

    if( port > 0 ) {
        JSValue jsv_port = JS_NewInt64(ctx, port);
        JS_SetPropertyStr(ctx, ent, "port", jsv_port);
    }

    return ent;
}

static JSValue js_net_shutdown(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    int result;
    int64_t fd;
    int32_t mode = SHUT_RDWR;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    if( argc > 1) {
        if( JS_ToInt32(ctx, &mode, argv[1]) )
            return JS_EXCEPTION;
    }

    if( -1 == shutdown(fd, mode) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    return JS_TRUE;
}

static const JSCFunctionListEntry js_net_funcs[] = {
    JS_CFUNC_DEF("accept", 1, js_net_accept ),
    JS_CFUNC_DEF("bind", 4, js_net_bind ),
    JS_CFUNC_DEF("connect", 4, js_net_connect ),
    JS_CFUNC_DEF("familyname", 1, js_net_familyname),
    JS_CFUNC_DEF("listen", 2, js_net_listen ),
    JS_CFUNC_DEF("resolve", 1, js_net_resolve ),
    JS_CFUNC_DEF("socket", 2, js_net_socket ),
    JS_CFUNC_DEF("socktypename", 1, js_net_socktypename),
    JS_CFUNC_DEF("shutdown", 2, js_net_shutdown ),
    JS_CFUNC_DEF("sync", 1, js_net_sync )
};

static int js_net_init(JSContext *ctx, JSModuleDef *m)
{
#ifdef USE_CARES
    JS_NewClassID(&js_net_resolve_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_net_resolve_class_id, &js_net_resolve_class);
#endif

    JS_SetModuleExportList(ctx, m, js_net_funcs,
                           countof(js_net_funcs));

    for (size_t i = 0; i < countof(jsnetconsts); i++) {
        JSValue v = JS_NewInt32(ctx, jsnetconsts[i].value);
        JS_SetModuleExport(ctx, m, jsnetconsts[i].name, v);
    }

    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_net_init);
    if (!m)
        return NULL;

    JS_AddModuleExportList(ctx, m, js_net_funcs, countof(js_net_funcs));
    for (size_t i = 0; i < countof(jsnetconsts); i++)
        JS_AddModuleExport(ctx, m, jsnetconsts[i].name);

    return m;
}

