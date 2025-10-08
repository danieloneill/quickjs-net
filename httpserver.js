import * as net from "net.so";
import * as std from "std";
import * as os from "os";

const sinfo = { 'family':'inet6', 'ip':'::', 'port':8081 };
const webroot = './webroot';

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

const serverfd = net.socket(sinfo.family, 'stream');
try {
    if( !net.bind(serverfd, sinfo.family, sinfo.ip, sinfo.port) )
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

console.log(`Listening on ${sinfo.family}:[${sinfo.ip}]:${sinfo.port}`);

function setupClient(info)
{
    console.log("info: "+JSON.stringify(info,null,2));

    os.setReadHandler(info.fd, function() {
        let data = new Uint8Array(8192);
        try {
            const br = os.read(info.fd, data.buffer, 0, data.length);
            if( br < 0 ) {
                console.log("read error");
                os.setReadHandler(info.fd, null);
                return;
            }
            else if( 0 == br )
            {
                console.log(`Connection closed from ${info.family}:[${info.ip}]:${info.port}`);
                close(info);
            }
            else
            {
                const asstr = uint8arrayToString(data);
                handlePacket(info, asstr);
                net.shutdown(info.fd, 'rd');
                os.setReadHandler(info.fd, null);
            }
        } catch(err) {
            console.log("readhandler: "+err);
            os.setReadHandler(info.fd, null);
        }
    });
}

function close(info)
{
    os.setReadHandler(info.fd, null);
    net.shutdown(info.fd);
    os.close(info.fd);
    console.log(`Closing connection from ${info.family}:[${info.ip}]:${info.port}`);
}

function sendFileChunk(info)
{
    if( !info )
    {
        console.log("sendFileChunk: no info!");
        return;
    }

    let data = new Uint8Array(1024);
    try {
        const br = os.read(info.file, data.buffer, 0, data.length);
        if( 0 == br )
        {
            os.setReadHandler(info.file, null);
            os.close(info.file);
            close(info);

            return;
        }

        os.write(info.fd, data.buffer, 0, br);
    } catch(err) {
        console.log("sendfile: "+err);
        console.log( new Error().stack );
        os.setReadHandler(info.file, null);
        os.close(info.file);
        close(info);
    }
}

function sendFile(info, path, mimetype)
{
    const [obj, err] = os.stat(path);
    if( !( obj.mode & os.S_IFREG ) )
    {
        console.log(' -- Not a file -- ');
        //send404(info);
        return;
    }

    info.file = os.open(path, os.O_RDONLY);
    const asstr = `HTTP/1.0 200 Ok\r\nContent-Type: ${mimetype}\r\nConnection: close\r\nContent-Length: ${obj.size}\r\n\r\n`;
    const newbuf = stringToUint8array(asstr);
    os.write(info.fd, newbuf.buffer, 0, newbuf.length);

    os.setReadHandler(info.file, function() {
        sendFileChunk(info);
    });
}

function sendContent(info, content, mimetype, fullstatus)
{
    if( !fullstatus )
        fullstatus = '200 OK';
    const asstr = `HTTP/1.0 ${fullstatus}\r\nConnection: close\r\nContent-Type: ${mimetype}\r\nContent-Length: ${content.length}\r\n\r\n${content}`;
    const newbuf = stringToUint8array(asstr);
    os.write(info.fd, newbuf.buffer, 0, newbuf.length);
    close(info);
}

function send404(info)
{
    console.log(" xxx Sending 404...");
    console.log( new Error().stack );
    const content = "<html><head><title>404 - Not Found</title></head><body><h2>Sorry, resource not found.</h2></body></html>";
    sendContent(info, content, 'text/html', '400 Not Found');
}

function handlePacket(info, pkt)
{
    const lines = pkt.split(/\r\n/);
    const req = lines[0].split(' ');

    console.log("Request: "+req[1]);
    if( !req || req.length < 3 )
        return;

    if( req[1] == '/' )
    {
        const content = "<html><head><title>QuickJS - QuickHTTPServer</title></head><body><h2>Success</h2><p /><a href=\"https://github.com/danieloneill/quickjs-net\"><img src=\"/github.png\" /></a></body></html>";
        const asstr = `HTTP/1.0 200 Ok\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ${content.length}\r\n\r\n${content}`;
        const newbuf = stringToUint8array(asstr);
        os.write(info.fd, newbuf.buffer, 0, newbuf.length);
        close(info)
    }
    else if( req[1] == '/github.png' )
        sendFile(info, 'github.png', 'image/png');
    else
        sendResource(info, webroot, req[1].substr(1));

    std.gc();
}

function sendResource(info, root, path)
{
    const fullpath = root + '/' + path;
    const [sobj, serr] = os.stat(fullpath);
    if( !sobj )
    {
        console.log(`404: ${fullpath}`);
        return send404(info);
    }

    if( sobj.mode & os.S_IFDIR )
        return sendContent(info, dirListing(root, path), 'text/html');
    else if( sobj.mode & os.S_IFREG )
    {
        //sendContent(info, "I'm a file.", 'text/html');
        let mimetype = 'text/plain';
        if( path.endsWith('.png') )
            mimetype = 'image/png';
        else if( path.endsWith('.jpg') )
            mimetype = 'image/jpg';
        else if( path.endsWith('.css') )
            mimetype = 'text/css';
        else if( path.endsWith('.js') )
            mimetype = 'text/javascript';
        else if( path.endsWith('.otf') )
            mimetype = 'font/otf';
        sendFile(info, fullpath, mimetype);
    }
    else
        sendContent(info, "I'm something else.", 'text/html');
        //sendContent(info, dirListing('/tmp', '/tmp'), 'text/html');
}

function dirListing(root, path)
{
    const fullpath = `${root}/${path}`;
    const [ents, rerr] = os.readdir(fullpath);

    let result = `<!DOCTYPE html>
<html>
    <head>
        <title>Listing of ${path}</title>
        <link rel="stylesheet" href="/internal/css/style.css"></link>
        <script src="/internal/js/gallery.js"></script>
        <link rel="stylesheet" href="/internal/css/fontawesome.min.css"></link>
        <link rel="stylesheet" href="/internal/css/all.min.css"></link>
    </head>
    <body onLoad="initKeys();">
        <div id="background"></div>
        <div id="content">
            <h1>Listing of ${path}</h1>
            <hr />
            <table width="100%">`;

    let images = [];
    let index = 0;

    let pathParts = path.split(/\//g).filter( function(p) { return p.length > 0; } );
    if( pathParts.length > 0 )
    {
        pathParts.pop();
        const prevPath = pathParts.join('/');
        result += `<tr>
            <td colspan="4"><a href="/${prevPath}">Up to /${prevPath}</a></td>
        </tr>`;
    }

    for( const e of ents )
    {
        if( e === '.' || e === '..' )
            continue;

        const filepath = `${fullpath}/${e}`;
        const [sobj, serr] = os.stat(filepath);
        let niceSize = '???';
        let mdate = '???';
        if( !sobj )
            niceSize = serr;
        else
        {
            niceSize = bytesToSize(sobj.size);
            mdate = new Date(sobj.mtime);
        }

        let imgLink = '';
        if( e.endsWith('.png') || e.endsWith('.jpg') || e.endsWith('.gif') )
        {
            images.push( `${path}/${e}` );
            imgLink = `<span onClick="openGallery(${index});" style="cursor: pointer;"><i class="fas fa-regular fa-images"></i></span>`;
            index++;
        }

        result += `<tr>
            <td><a href="${path}/${e}">${e}</a></td>
            <td>${niceSize}</td>
            <td>${mdate}</td>
            <td>${imgLink}</td>
        </tr>`;
    }
    result += `</table></div>
    <script>const images = ${JSON.stringify(images)};</script>
    <div id="imageViewer" onClick="closeGallery();"><img onClick="closeGallery();" id="focusImage" /><br /><span id="imageLabel"></span></div>
    </body></html>`;

    return result;
}

function bytesToSize(bytes) {
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB']
  if (bytes === 0) return 'n/a'
  const i = parseInt(Math.floor(Math.log(bytes) / Math.log(1024)), 10)
  if (i === 0) return `${bytes} ${sizes[i]})`
  return `${(bytes / (1024 ** i)).toFixed(1)} ${sizes[i]}`
}

