import * as net from "net.so";
import * as std from "std";
import * as os from "os";

const sinfo = { 'domain':'inet6', 'addr':'::', 'port':54321 };

function uint8arrayToString(arr)
{
    const enc = String.fromCharCode.apply(null, arr);
    const dec = decodeURIComponent(escape(enc));
    return dec;
}

function stringToUint8array(str)
{
    const encstr = unescape(encodeURIComponent(str));
    const chrlist = encstr.split('');
    const arr = chrlist.map( ch => ch.charCodeAt(0) );
    return new Uint8Array(arr);
}

const serverfd = net.socket(sinfo.domain, 'stream');
try {
    if( !net.bind(serverfd, sinfo.domain, sinfo.addr, sinfo.port) )
    {
        console.log('failed to bind');
        std.exit(-1);
    }
} catch(err) {
    console.log("bind: "+err);
    std.exit(-1);
}

try {
    if( !net.listen(serverfd, 10) )
    {
        console.log('failed to listen');
        std.exit(-1);
    }
} catch(err) {
    console.log("listen: "+err);
    std.exit(-1);
}

let handles = {};
os.setReadHandler(serverfd, function() {
    try {
        const info = net.accept(serverfd);
        console.log(`Connection from ${info.family}:[${info.ip}]:${info.port}`);
        setupClient(info);
    } catch(err) {
        console.log("accept: "+err);
        std.exit(-1);
    }
});

console.log(`Listening on ${sinfo.domain}:[${sinfo.addr}]:${sinfo.port}`);

function setupClient(info)
{
    const fd = info.fd;

    os.setReadHandler(fd, function() {
        let data = new Uint8Array(1024);
        try {
            const br = os.read(fd, data.buffer, 0, data.length);
            if( br < 0 ) {
                console.log("read error");
                os.setReadHandler(fd, null);
                return;
            }
            else if( 0 == br )
            {
                console.log(`Connection closed from ${info.family}:[${info.ip}]:${info.port}`);
                os.setReadHandler(fd, null);
                net.close(fd);
            }
            else
            {
                const asstr = uint8arrayToString(data);
                console.log("Read["+br+"]: "+asstr);
    
                // send it back
                const newbuf = stringToUint8array(asstr);
                os.write(fd, newbuf.buffer, 0, newbuf.length);
            }
        } catch(err) {
            console.log("readhandler: "+err);
            os.setReadHandler(fd, null);
        }
    });
}

