# Mock of the TV's web server, for testing the phone page without hardware.
import http.server, json, os, re, sys, urllib.parse
ROOT=sys.argv[1]; STORE=sys.argv[2]; os.makedirs(STORE,exist_ok=True)
LIGHT={'level':255}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self,*a): pass
    def _send(self,code,ctype,body):
        if isinstance(body,str): body=body.encode()
        self.send_response(code); self.send_header('Content-Type',ctype); self.send_header('Content-Length',str(len(body))); self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        u=urllib.parse.urlparse(self.path); q=urllib.parse.parse_qs(u.query)
        if u.path=='/': return self._send(200,'text/html',open(os.path.join(ROOT,'index.html'),'rb').read())
        if u.path=='/list':
            clips=[{'n':f,'s':os.path.getsize(os.path.join(STORE,f))} for f in sorted(os.listdir(STORE)) if f.endswith('.mjpeg')]
            return self._send(200,'application/json',json.dumps({'clips':clips,'now':clips[-1]['n'] if clips else ''}))
        if u.path=='/delete':
            n=q.get('name',[''])[0]; p=os.path.join(STORE,os.path.basename(n))
            if os.path.exists(p): os.remove(p); return self._send(200,'text/plain','OK')
            return self._send(404,'text/plain','No such clip')
        if u.path=='/play': return self._send(200,'text/plain','OK')
        if u.path=='/light':
            if 'level' in q: LIGHT['level']=int(q['level'][0])
            return self._send(200,'application/json',json.dumps({'level':LIGHT['level'],'now':LIGHT['level']}))
        if u.path=='/_light': return self._send(200,'application/json',json.dumps(LIGHT))
        self._send(404,'text/plain','Not found')
    def do_POST(self):
        u=urllib.parse.urlparse(self.path); q=urllib.parse.parse_qs(u.query)
        if u.path!='/upload': return self._send(404,'text/plain','Not found')
        body=self.rfile.read(int(self.headers['Content-Length']))
        bnd=('--'+self.headers['Content-Type'].split('boundary=')[1]).encode()
        part=body.split(bnd)[1]; head,_,data=part.partition(b'\r\n\r\n'); data=data[:-2]
        assert b'name="file"' in head
        name=re.sub(r'[^A-Za-z0-9_-]','_',q.get('name',['clip'])[0])[:20] or 'clip'; fps=int(q.get('fps',['0'])[0])
        fn=f"{name}.f{fps}.mjpeg" if fps else f"{name}.mjpeg"
        open(os.path.join(STORE,fn),'wb').write(data)
        assert int(q.get('size',['0'])[0])==len(data),'size mismatch'
        self._send(200,'text/plain','OK')
http.server.ThreadingHTTPServer(('127.0.0.1',8765),H).serve_forever()
