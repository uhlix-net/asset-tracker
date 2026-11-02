"""A local stand-in for the three Firebase REST hosts, for the C++ sync tests.

    python mock_firebase.py <port>

Lists page aggressively so pagination is exercised. GET /__dump returns everything
stored, base64 where binary; POST /__reset clears it.
"""
import base64
import json
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = 2
TOKEN, UID = "test-token", "uid1"
docs, objects = {}, {}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def reply(self, status, body=b"", kind="application/json"):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length") or 0))

    def authorized(self):
        if self.headers.get("Authorization") == f"Bearer {TOKEN}":
            return True
        self.reply(401, {"error": {"message": "UNAUTHENTICATED"}})
        return False

    def dispatch(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query)
        body = self.body()
        path = url.path

        if path == "/__dump":
            return self.reply(200, {"docs": docs, "objects": {
                k: base64.b64encode(v).decode() for k, v in objects.items()}})
        if path == "/__reset":
            docs.clear(), objects.clear()
            return self.reply(200, {})

        if path == "/auth":
            sent = json.loads(body)
            if query.get("key") != ["test-api-key"] or sent["password"] != "fb-password":
                return self.reply(400, {"error": {"message": "INVALID_LOGIN_CREDENTIALS"}})
            return self.reply(200, {"idToken": TOKEN, "localId": UID})

        if not self.authorized():
            return None

        marker = "/documents/users/%s/" % UID
        if path.startswith("/firestore/") and marker in path:
            return self.firestore(path.split(marker, 1)[1], query, body)

        prefix = "/storage/b/bucket/o"
        if path == prefix and self.command == "POST":
            objects[query["name"][0]] = body
            return self.reply(200, {"name": query["name"][0]})
        if path == prefix and self.command == "GET":
            names = sorted(n for n in objects if n.startswith(query.get("prefix", [""])[0]))
            start = int(query.get("pageToken", ["0"])[0])
            page = {"items": [{"name": n} for n in names[start:start + PAGE]]}
            if start + PAGE < len(names):
                page["nextPageToken"] = str(start + PAGE)
            return self.reply(200, page)
        if path.startswith(prefix + "/"):
            # Path segments must arrive encoded, or the name would not be one segment.
            raw = self.path.split("?", 1)[0][len(prefix) + 1:]
            name = urllib.parse.unquote(raw)
            if "/" in raw or name not in objects:
                return self.reply(404, {"error": {"message": "not found"}})
            if self.command == "DELETE":
                del objects[name]
                return self.reply(204)
            return self.reply(200, objects[name], "application/octet-stream")
        return self.reply(404, {"error": {"message": "no route " + path}})

    def firestore(self, rel, query, body):
        if "/" in rel:
            if self.command == "PATCH":
                docs[rel] = json.loads(body)["fields"]
                return self.reply(200, {"name": rel, "fields": docs[rel]})
            if rel not in docs:
                return self.reply(404, {"error": {"message": "not found"}})
            if self.command == "DELETE":
                del docs[rel]
                return self.reply(200, {})
            return self.reply(200, {"name": "x/" + rel, "fields": docs[rel]})

        names = sorted(k for k in docs if k.startswith(rel + "/"))
        start = int(query.get("pageToken", ["0"])[0])
        page = {"documents": [{"name": "projects/x/" + k, "fields": docs[k]}
                              for k in names[start:start + PAGE]]}
        if start + PAGE < len(names):
            page["nextPageToken"] = str(start + PAGE)
        return self.reply(200, page if names else {})

    do_GET = do_POST = do_PATCH = do_DELETE = dispatch


ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler).serve_forever()
