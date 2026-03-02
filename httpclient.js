import * as net from "net.so";
import * as std from "std";
import * as os from "os";

const outfilename = 'out.jpg';

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

let inheaders = true;
let written = 0;
let curline = [];
let lines = [];
let fout = os.open(outfilename, (os.O_CREAT | os.O_TRUNC | os.O_WRONLY), 0o777);

const handle = net.socket(net.AF_INET, net.SOCK_STREAM);
os.setReadHandler(handle.fd, function() {
    let data = new Uint8Array(1024);
    try {
        const br = os.read(handle.fd, data.buffer, 0, data.length);
        if( br < 0 ) {
            console.log("read error");
            os.setReadHandler(handle.fd, null);
            return;
        }
        else if( 0 == br )
        {
            console.log('closing...');
            os.setReadHandler(handle.fd, null);
            net.shutdown(handle);
            os.close(handle.fd);

            os.close(fout);

            console.log(`saved ${written} bytes to ${outfilename}`);
            std.exit(0);
        }
        else
        {
            if( !inheaders )
                return (written += os.write(fout, data.buffer, 0, br));

            for( let x=0; x < br; x++ )
            {
                const ch = data[x];
                if( ch == 13 && curline.length == 1 && curline[0] == 10 )
                {
                    console.log('done headers');
                    inheaders = false;
                    if( x < br-1 )
                    {
                            const headers = lines.map( line => uint8arrayToString(line).trim() );
                            console.log( "Headers: "+JSON.stringify(headers,null,2) );

                        const sa = data.slice(x+2, br);
                        written += os.write(fout, sa.buffer, 0, sa.length);
                        return;
                    }
                }
                else
                {
                    curline.push(ch);
                    if( ch == 13 )
                    {
                        lines.push(curline);
                        curline = [];
                    }
                }
            }
        }
    } catch(err) {
        console.log("readhandler: "+err);
        os.setReadHandler(handle.fd, null);
        std.exit(-1);
    }
});

let iplist;
try {
    iplist = net.resolve("cogconnected.com", net.AF_INET);
    console.log(JSON.stringify(iplist, null, 2));
} catch(err) {
    console.log("resolve: "+err);
    std.exit(-1);
}

if( !net.connect(handle, iplist[0].ip, 80) )
{
    console.log("no connect");
}
else
{
    console.log("connected");
    const reqstr = 'GET /wp-content/uploads/2020/12/anastasiya-judy-cyberpunk3-min-560x700.jpg HTTP/1.0\r\nConnection: close\r\nHost: cogconnected.com\r\n\r\n';
    const newbuf = stringToUint8array(reqstr);
    os.write(handle.fd, newbuf.buffer, 0, newbuf.length);
}
