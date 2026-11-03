"""A local stand-in for the Firebase REST hosts, for the C++ sync tests.

    python mock_firebase.py <port>

Accounts work as in Firebase: sign up, sign in, password reset, password change
and deletion, each user confined to users/{their uid}/. Lists page aggressively so
pagination is exercised. GET /__dump returns everything stored (binary as base64)
plus counters; POST /__reset restores the starting state.
"""
import base64
import json
import re
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = 2
API_KEY = "test-api-key"
state = {}


def reset():
    state.clear()
    state.update(users={"a@b.c": {"password": "fb-password", "uid": "uid1"}},
                 tokens={}, docs={}, objects={}, resets=[], uploads=0, writes=0, next_uid=2)


reset()


def error(message):
    return {"error": {"message": message}}


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

    def session(self, user):
        token = f"tok-{user['uid']}-{len(state['tokens'])}"
        state["tokens"][token] = user["uid"]
        return {"idToken": token, "localId": user["uid"], "email": self.email_of(user["uid"])}

    @staticmethod
    def email_of(uid):
        return next((e for e, u in state["users"].items() if u["uid"] == uid), "")

    def caller(self):
        header = self.headers.get("Authorization", "")
        return state["tokens"].get(header.removeprefix("Bearer "))

    def dispatch(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query)
        raw = self.rfile.read(int(self.headers.get("Content-Length") or 0))
        path = url.path

        if path == "/__dump":
            return self.reply(200, {**{k: state[k] for k in
                                       ("docs", "resets", "uploads", "writes", "users")},
                                    "objects": {k: base64.b64encode(v).decode()
                                                for k, v in state["objects"].items()}})
        if path == "/__reset":
            reset()
            return self.reply(200, {})

        if path.startswith("/auth/"):
            if query.get("key") != [API_KEY]:
                return self.reply(400, error("API key not valid. Please pass a valid API key."))
            return self.auth(path[len("/auth/"):], json.loads(raw or b"{}"))

        uid = self.caller()
        if not uid:
            return self.reply(401, error("UNAUTHENTICATED"))

        match = re.search(r"/documents/users/([^/]+)/(.*)$", path)
        if path.startswith("/firestore/") and match:
            if match.group(1) != uid:
                return self.reply(403, error("PERMISSION_DENIED"))
            return self.firestore(f"{uid}/{match.group(2)}", query, raw)

        prefix = "/storage/b/bucket/o"
        mine = f"users/{uid}/"
        if path == prefix and self.command == "POST":
            name = query["name"][0]
            if not name.startswith(mine):
                return self.reply(403, error("PERMISSION_DENIED"))
            state["objects"][name] = raw
            state["uploads"] += 1
            return self.reply(200, {"name": name})
        if path == prefix and self.command == "GET":
            wanted = query.get("prefix", [""])[0]
            if not wanted.startswith(mine):
                return self.reply(403, error("PERMISSION_DENIED"))
            names = sorted(n for n in state["objects"] if n.startswith(wanted))
            start = int(query.get("pageToken", ["0"])[0])
            page = {"items": [{"name": n} for n in names[start:start + PAGE]]}
            if start + PAGE < len(names):
                page["nextPageToken"] = str(start + PAGE)
            return self.reply(200, page)
        if path.startswith(prefix + "/"):
            # Path segments must arrive encoded, or the name would not be one segment.
            encoded = self.path.split("?", 1)[0][len(prefix) + 1:]
            name = urllib.parse.unquote(encoded)
            if not name.startswith(mine):
                return self.reply(403, error("PERMISSION_DENIED"))
            if "/" in encoded or name not in state["objects"]:
                return self.reply(404, error("not found"))
            if self.command == "DELETE":
                del state["objects"][name]
                return self.reply(204)
            return self.reply(200, state["objects"][name], "application/octet-stream")
        return self.reply(404, error("no route " + path))

    def auth(self, operation, body):
        users = state["users"]
        if operation == "signInWithPassword":
            user = users.get(body.get("email", ""))
            if not user or user["password"] != body.get("password"):
                return self.reply(400, error("INVALID_LOGIN_CREDENTIALS"))
            return self.reply(200, self.session(user))
        if operation == "signUp":
            email, password = body.get("email", ""), body.get("password", "")
            if "@" not in email:
                return self.reply(400, error("INVALID_EMAIL"))
            if email in users:
                return self.reply(400, error("EMAIL_EXISTS"))
            if len(password) < 6:
                return self.reply(400, error("WEAK_PASSWORD : Password should be at least 6 characters"))
            users[email] = {"password": password, "uid": f"uid{state['next_uid']}"}
            state["next_uid"] += 1
            return self.reply(200, self.session(users[email]))
        if operation == "sendOobCode":
            if body.get("requestType") != "PASSWORD_RESET":
                return self.reply(400, error("INVALID_REQ_TYPE"))
            state["resets"].append(body.get("email", ""))
            return self.reply(200, {"email": body.get("email", "")})

        uid = state["tokens"].get(body.get("idToken", ""))
        if not uid:
            return self.reply(400, error("INVALID_ID_TOKEN"))
        email = self.email_of(uid)
        if operation == "update":
            if len(body.get("password", "")) < 6:
                return self.reply(400, error("WEAK_PASSWORD"))
            users[email]["password"] = body["password"]
            return self.reply(200, self.session(users[email]))
        if operation == "delete":
            del users[email]
            state["tokens"] = {t: u for t, u in state["tokens"].items() if u != uid}
            return self.reply(200, {})
        return self.reply(404, error("no auth route " + operation))

    def firestore(self, rel, query, raw):
        docs = state["docs"]
        if rel.count("/") >= 2:
            if self.command == "PATCH":
                docs[rel] = json.loads(raw)["fields"]
                state["writes"] += 1
                return self.reply(200, {"name": rel, "fields": docs[rel]})
            if rel not in docs:
                return self.reply(404, error("not found"))
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
