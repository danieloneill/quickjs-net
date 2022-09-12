#include "quickjs-net.h"
#include "cutils.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

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

static JSValue js_net_socket(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    const char *domain, *socktype = NULL;
    int fd;
    int err;

    domain = JS_ToCString(ctx, argv[0]);
    if (!domain)
        goto fail;
    socktype = JS_ToCString(ctx, argv[1]);
    if (!socktype)
        goto fail;

    int intdomain = net_domain_to_int(domain);;
    if( -1 == intdomain )
    {
        JS_ThrowTypeError(ctx, "invalid domain");
        goto fail;
    }

    int inttype = net_socktype_to_int(socktype);
    if( -1 == inttype )
    {
        JS_ThrowTypeError(ctx, "invalid type");
        goto fail;
    }

    fd = socket(intdomain, inttype, 0);
    if (-1 == fd)
        err = errno;
    else
        err = 0;
    if (argc >= 3)
        js_set_error_object(ctx, argv[2], err);
    JS_FreeCString(ctx, domain);
    JS_FreeCString(ctx, socktype);
    if (-1 == fd)
        return JS_NULL;
    return JS_NewInt64(ctx, fd);
 fail:
    JS_FreeCString(ctx, domain);
    JS_FreeCString(ctx, socktype);
    return JS_EXCEPTION;
}

static JSValue js_net_resolve(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    const char *node = NULL, *family = NULL;
    int s;
    int err;

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    node = JS_ToCString(ctx, argv[0]);
    if (!node)
    {
        JS_ThrowTypeError(ctx, "node must be specified");
        goto fail;
    }

    if( argc >= 2 )
        family = JS_ToCString(ctx, argv[1]);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    //hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    if( family )
    {
        if( strcasecmp(family, "unspec") == 0 )
            hints.ai_family = AF_UNSPEC;
        else if( strcasecmp(family, "inet") == 0 )
            hints.ai_family = AF_INET;
        else if( strcasecmp(family, "inet6") == 0 )
            hints.ai_family = AF_INET6;
        else
        {
            JS_ThrowTypeError(ctx, "invalid address family");
            goto fail;
        }
    }

    s = getaddrinfo(node, NULL, &hints, &result);
    if( 0 != s )
    {
        JS_ThrowReferenceError(ctx, "%s", gai_strerror(s));
        goto fail;
    }

    JSValue resultList = JS_NewArray(ctx);
    int resultIndex = 0;
    for( rp=result; rp != NULL; rp = rp->ai_next )
    {
        char outip[BUFSIZ];
        void *ptr = NULL;

        if( rp->ai_family == PF_INET )
            ptr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr;
        else if( rp->ai_family == PF_INET6 )
            ptr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr;

        const char *r = inet_ntop(rp->ai_family, ptr, outip, BUFSIZ-1);
        if( NULL == r )
        {
            printf("err: %s\n", strerror(errno));
            continue;
        }

        JSValue jsv_family = JS_NewString(ctx, net_int_to_domain(rp->ai_family));
        JSValue jsv_type = JS_NewString(ctx, net_int_to_socktype(rp->ai_socktype));
        JSValue jsv_ip = JS_NewString(ctx, outip);

        JSValue ent = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ent, "family", jsv_family);
        JS_SetPropertyStr(ctx, ent, "type", jsv_type);
        JS_SetPropertyStr(ctx, ent, "ip", jsv_ip);

        JS_SetPropertyUint32(ctx, resultList, resultIndex++, ent);
    }
    freeaddrinfo(result);

    JS_FreeCString(ctx, node);
    if( family )
        JS_FreeCString(ctx, family);
    return resultList;
fail:
    JS_FreeCString(ctx, node);
    if( family )
        JS_FreeCString(ctx, family);
    return JS_EXCEPTION;
}

static JSValue js_net_connect(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    unsigned char addrbuf[sizeof(struct in6_addr)];
    const char *family=NULL, *addr=NULL;
    int64_t fd, port;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    family = JS_ToCString(ctx, argv[1]);
    if (!family)
    {
        JS_ThrowTypeError(ctx, "family must be specified");
        goto fail;
    }

    addr = JS_ToCString(ctx, argv[2]);
    if (!addr)
    {
        JS_ThrowTypeError(ctx, "addr must be specified");
        goto fail;
    }

    if( JS_ToInt64(ctx, &port, argv[3]) )
        return JS_EXCEPTION;

    int int_family = net_domain_to_int(family);
    if( -1 == int_family )
    {
        JS_ThrowTypeError(ctx, "unsupported family");
        goto fail;
    }

    if( -1 == inet_pton(int_family, addr, &addrbuf) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        goto fail;
    }

    if( AF_INET == int_family )
    {
        struct sockaddr_in sa;
        memset( &sa, 0, sizeof(sa) );
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        memcpy( &sa.sin_addr, &addrbuf, sizeof(struct in_addr) );
        
        if( -1 == connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }
    else if( AF_INET6 == int_family )
    {
        struct sockaddr_in6 sa;
        memset( &sa, 0, sizeof(sa) );
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(port);
        memcpy( &sa.sin6_addr, &addrbuf, sizeof(struct in6_addr) );
        
        if( -1 == connect(fd, (struct sockaddr *)&sa, sizeof(struct in6_addr)) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }

    JS_FreeCString(ctx, family);
    JS_FreeCString(ctx, addr);
    return JS_TRUE;
fail:
    JS_FreeCString(ctx, family);
    JS_FreeCString(ctx, addr);
    return JS_FALSE;
}

static JSValue js_net_bind(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    unsigned char addrbuf[sizeof(struct in6_addr)];
    const char *family=NULL, *addr=NULL;
    int64_t fd, port;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    family = JS_ToCString(ctx, argv[1]);
    if (!family)
    {
        JS_ThrowTypeError(ctx, "family must be specified");
        goto fail;
    }

    addr = JS_ToCString(ctx, argv[2]);
    if (!addr)
    {
        JS_ThrowTypeError(ctx, "addr must be specified");
        goto fail;
    }

    if( JS_ToInt64(ctx, &port, argv[3]) )
        return JS_EXCEPTION;

    int int_family = net_domain_to_int(family);
    if( -1 == int_family )
    {
        JS_ThrowTypeError(ctx, "unsupported family");
        goto fail;
    }

    if( -1 == inet_pton(int_family, addr, &addrbuf) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        goto fail;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(int));

    if( AF_INET == int_family )
    {
        struct sockaddr_in sa;
        memset( &sa, 0, sizeof(sa) );
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        memcpy( &sa.sin_addr, &addrbuf, sizeof(struct in_addr) );
        
        if( -1 == bind(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }
    else if( AF_INET6 == int_family )
    {
        struct sockaddr_in6 sa;
        memset( &sa, 0, sizeof(sa) );
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(port);
        memcpy( &sa.sin6_addr, &addrbuf, sizeof(struct in6_addr) );
        
        if( -1 == bind(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in6)) )
        {
            JS_ThrowTypeError(ctx, "%s", strerror(errno));
            goto fail;
        }
    }

    JS_FreeCString(ctx, family);
    JS_FreeCString(ctx, addr);
    return JS_TRUE;
fail:
    JS_FreeCString(ctx, family);
    JS_FreeCString(ctx, addr);
    return JS_FALSE;
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
    struct sockaddr_in6 sockaddr;
    socklen_t socklen = sizeof(struct sockaddr_in6);
    int64_t infd, outfd, port;
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

    if( sockaddr.sin6_family == PF_INET )
    {
        ptr = &((struct sockaddr_in *)&sockaddr)->sin_addr;
        port = ntohs( ((struct sockaddr_in *)&sockaddr)->sin_port );
    }
    else if( sockaddr.sin6_family == PF_INET6 )
    {
        ptr = &sockaddr.sin6_addr;
        port = ntohs( sockaddr.sin6_port );
    }

    const char *r = inet_ntop(sockaddr.sin6_family, ptr, outip, BUFSIZ-1);
    if( NULL == r )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    JSValue jsv_family = JS_NewString(ctx, net_int_to_domain(sockaddr.sin6_family));
    JSValue jsv_ip = JS_NewString(ctx, outip);
    JSValue jsv_port = JS_NewInt64(ctx, port);
    JSValue jsv_fd = JS_NewInt64(ctx, outfd);

    JSValue ent = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ent, "family", jsv_family);
    JS_SetPropertyStr(ctx, ent, "ip", jsv_ip);
    JS_SetPropertyStr(ctx, ent, "port", jsv_port);
    JS_SetPropertyStr(ctx, ent, "fd", jsv_fd);

    return ent;
}

static JSValue js_net_close(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    int result;
    int64_t fd;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    if( -1 == close(fd) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    return JS_TRUE;
}

static JSValue js_net_shutdown(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    const char *mode = NULL;
    int result;
    int64_t fd;

    if( JS_ToInt64(ctx, &fd, argv[0]) )
        return JS_EXCEPTION;

    mode = JS_ToCString(ctx, argv[1]);
    if (!mode)
    {
        JS_ThrowTypeError(ctx, "mode must be specified");
        JS_FreeCString(ctx, mode);
        return JS_FALSE;
    }

    int intmode = -1;
    if( strcasecmp(mode, "rd") == 0 ) intmode = SHUT_RD;
    else if( strcasecmp(mode, "wr") == 0 ) intmode = SHUT_WR;
    else if( strcasecmp(mode, "rdwr") == 0 ) intmode = SHUT_RDWR;
    JS_FreeCString(ctx, mode);

    if( -1 == intmode )
    {
        JS_ThrowTypeError(ctx, "mode must be one of 'rd', 'wr', or 'rdwr'");
        return JS_FALSE;
    }

    if( -1 == shutdown(fd, intmode) )
    {
        JS_ThrowTypeError(ctx, "%s", strerror(errno));
        return JS_UNDEFINED;
    }

    return JS_TRUE;
}

static const JSCFunctionListEntry js_net_funcs[] = {
    JS_CFUNC_DEF("socket", 2, js_net_socket ),
    JS_CFUNC_DEF("resolve", 1, js_net_resolve ),
    JS_CFUNC_DEF("connect", 4, js_net_connect ),
    JS_CFUNC_DEF("bind", 3, js_net_bind ),
    JS_CFUNC_DEF("listen", 2, js_net_listen ),
    JS_CFUNC_DEF("accept", 1, js_net_accept ),
    JS_CFUNC_DEF("close", 1, js_net_close ),
    JS_CFUNC_DEF("shutdown", 2, js_net_shutdown ),
};

static int js_net_init(JSContext *ctx, JSModuleDef *m)
{
#ifdef USE_CARES
    JS_NewClassID(&js_net_resolve_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_net_resolve_class_id, &js_net_resolve_class);
#endif

    JS_SetModuleExportList(ctx, m, js_net_funcs,
                           countof(js_net_funcs));
    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_net_init);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, js_net_funcs, countof(js_net_funcs));
    return m;
}

