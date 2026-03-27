#!/usr/bin/env python3
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Any
from urllib.parse import unquote, urlparse


KEYWORDS = [
    "fuc",
    "return",
    "for",
    "in",
    "while",
    "do",
    "if",
    "elif",
    "else",
    "and",
    "or",
    "not",
    "import",
    "export",
    "from",
    "as",
    "type",
    "interface",
    "imut",
    "break",
    "continue",
    "defer",
    "kostroutine",
    "select",
    "match",
    "case",
    "default",
]

BUILTIN_CALL_PARAMS: dict[str, list[str]] = {
    "fs.read": ["path"],
    "fs.write": ["path", "text"],
    "fs.append": ["path", "text"],
    "fs.exists": ["path"],
    "fs.mkdir": ["path"],
    "fs.remove": ["path"],
    "time.sleep": ["ms"],
    "rand.seed": ["seed"],
    "rand.int": ["min", "max"],
    "http.get": ["url"],
    "http.delete": ["url"],
    "http.post": ["url", "body"],
    "http.put": ["url", "body"],
    "http.request": ["method", "url", "body"],
    "http.requestEx": ["method", "url", "body"],
}

BUILTIN_SIGNATURE_LABELS: dict[str, str] = {
    "fs.read": "fs.read(path)",
    "fs.write": "fs.write(path, text)",
    "fs.append": "fs.append(path, text)",
    "fs.exists": "fs.exists(path)",
    "fs.mkdir": "fs.mkdir(path)",
    "fs.remove": "fs.remove(path)",
    "time.sleep": "time.sleep(ms)",
    "rand.seed": "rand.seed(seed)",
    "rand.int": "rand.int(min, max)",
    "http.get": "http.get(url)",
    "http.delete": "http.delete(url)",
    "http.post": "http.post(url, body)",
    "http.put": "http.put(url, body)",
    "http.request": "http.request(method, url, body)",
    "http.requestEx": "http.requestEx(method, url, body)",
}


@dataclass
class SymbolDef:
    name: str
    line: int
    col: int
    kind: int
    type_info: str


@dataclass
class Document:
    uri: str
    text: str
    defs_by_name: dict[str, list[SymbolDef]]
    func_params_by_name: dict[str, list[str]]


class JsonRpc:
    def __init__(self) -> None:
        self.stdin = sys.stdin.buffer
        self.stdout = sys.stdout.buffer

    def read_message(self) -> dict[str, Any] | None:
        headers: dict[str, str] = {}
        while True:
            line = self.stdin.readline()
            if not line:
                return None
            if line in (b"\r\n", b"\n"):
                break
            try:
                h = line.decode("utf-8").strip()
            except UnicodeDecodeError:
                return None
            if ":" not in h:
                continue
            key, val = h.split(":", 1)
            headers[key.strip().lower()] = val.strip()

        if "content-length" not in headers:
            return None
        try:
            length = int(headers["content-length"])
        except ValueError:
            return None

        body = self.stdin.read(length)
        if not body:
            return None
        try:
            return json.loads(body.decode("utf-8"))
        except Exception:
            return None

    def write(self, payload: dict[str, Any]) -> None:
        data = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        header = f"Content-Length: {len(data)}\r\n\r\n".encode("ascii")
        self.stdout.write(header)
        self.stdout.write(data)
        self.stdout.flush()

    def respond(self, req_id: Any, result: Any) -> None:
        self.write({"jsonrpc": "2.0", "id": req_id, "result": result})

    def respond_error(self, req_id: Any, code: int, message: str) -> None:
        self.write({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}})

    def notify(self, method: str, params: Any) -> None:
        self.write({"jsonrpc": "2.0", "method": method, "params": params})


def uri_to_path(uri: str) -> str | None:
    p = urlparse(uri)
    if p.scheme != "file":
        return None
    path = unquote(p.path)
    if os.name == "nt" and path.startswith("/") and len(path) > 3 and path[2] == ":":
        path = path[1:]
    return path


def get_word_at(text: str, line: int, char: int) -> str | None:
    lines = text.splitlines()
    if line < 0 or line >= len(lines):
        return None
    s = lines[line]
    if not s:
        return None
    if char < 0:
        char = 0
    if char >= len(s):
        char = len(s) - 1
    if not (s[char].isalnum() or s[char] == "_"):
        if char > 0 and (s[char - 1].isalnum() or s[char - 1] == "_"):
            char -= 1
        else:
            return None

    lo = char
    hi = char
    while lo > 0 and (s[lo - 1].isalnum() or s[lo - 1] == "_"):
        lo -= 1
    while hi + 1 < len(s) and (s[hi + 1].isalnum() or s[hi + 1] == "_"):
        hi += 1
    return s[lo : hi + 1]


def parse_param_names(param_text: str) -> list[str]:
    params: list[str] = []
    chunks: list[str] = []
    start = 0
    depth_paren = 0
    depth_bracket = 0
    depth_brace = 0
    in_string = False
    quote = ""
    escaped = False

    for i, ch in enumerate(param_text):
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                in_string = False
            continue
        if ch in ('"', "'"):
            in_string = True
            quote = ch
            continue
        if ch == "(":
            depth_paren += 1
            continue
        if ch == ")":
            depth_paren = max(depth_paren - 1, 0)
            continue
        if ch == "[":
            depth_bracket += 1
            continue
        if ch == "]":
            depth_bracket = max(depth_bracket - 1, 0)
            continue
        if ch == "{":
            depth_brace += 1
            continue
        if ch == "}":
            depth_brace = max(depth_brace - 1, 0)
            continue
        if ch == "," and depth_paren == 0 and depth_bracket == 0 and depth_brace == 0:
            chunks.append(param_text[start:i])
            start = i + 1
    chunks.append(param_text[start:])

    for raw in chunks:
        item = raw.strip()
        if not item or item == "*":
            continue
        if ":" not in item:
            continue
        name = item.split(":", 1)[0].strip()
        if re.match(r"^[A-Za-z_]\w*$", name):
            params.append(name)
    return params


def find_call_context(line_prefix: str) -> tuple[str, str] | None:
    stack: list[int] = []
    in_string = False
    quote = ""
    escaped = False

    for i, ch in enumerate(line_prefix):
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                in_string = False
            continue

        if ch in ('"', "'"):
            in_string = True
            quote = ch
            continue
        if ch == "#":
            break
        if ch == "(":
            stack.append(i)
            continue
        if ch == ")":
            if stack:
                stack.pop()
            continue

    if not stack:
        return None

    open_idx = stack[-1]
    j = open_idx - 1
    while j >= 0 and line_prefix[j].isspace():
        j -= 1
    if j < 0:
        return None

    k = j
    while k >= 0 and (line_prefix[k].isalnum() or line_prefix[k] in "._"):
        k -= 1
    func_name = line_prefix[k + 1 : j + 1].strip()
    if not func_name:
        return None

    arg_prefix = line_prefix[open_idx + 1 :]
    return (func_name, arg_prefix)


def infer_expr_type(expr: str) -> str:
    e = expr.strip()
    if not e:
        return "unknown"
    if e in ("true", "false", "True", "False"):
        return "bool"
    if re.match(r"^-?\d+$", e):
        return "int"
    if re.match(r"^-?\d+\.\d+$", e):
        return "float"
    if (e.startswith('"') and e.endswith('"')) or (e.startswith("'") and e.endswith("'")):
        return "str"
    if e.startswith("[") and e.endswith("]"):
        return "list"
    if e.startswith("{") and e.endswith("}"):
        return "map"
    if e.startswith("int("):
        return "int"
    if e.startswith("float("):
        return "float"
    if e.startswith("str("):
        return "str"
    return "unknown"


FUNC_RE = re.compile(
    r"^\s*fuc\s+(?:\([A-Za-z_]\w*\s*:\s*[^)]+\)\s+)?([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+))?:"
)
TYPE_RE = re.compile(r"^\s*type\s+([A-Za-z_]\w*)\s*:")
IFACE_RE = re.compile(r"^\s*interface\s+([A-Za-z_]\w*)\s*:")
CONST_RE = re.compile(r"^\s*imut\s+([A-Za-z_]\w*)\s*=\s*(.+)$")
ASSIGN_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(.+)$")
FOR_RE = re.compile(r"^\s*for\s+([A-Za-z_]\w*)(?:\s*,\s*([A-Za-z_]\w*))?\s+in\s+(.+):\s*$")


def parse_symbols(text: str) -> dict[str, list[SymbolDef]]:
    defs: dict[str, list[SymbolDef]] = {}

    def add(name: str, line: int, col: int, kind: int, type_info: str) -> None:
        defs.setdefault(name, []).append(SymbolDef(name, line, col, kind, type_info))

    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = FUNC_RE.match(line)
        if m:
            name = m.group(1)
            params = m.group(2).strip()
            ret = (m.group(3) or "void").strip()
            add(name, i, line.find(name), 3, f"func({params}) -> {ret}")
            continue

        m = TYPE_RE.match(line)
        if m:
            name = m.group(1)
            add(name, i, line.find(name), 22, "type")
            continue

        m = IFACE_RE.match(line)
        if m:
            name = m.group(1)
            add(name, i, line.find(name), 11, "interface")
            continue

        m = CONST_RE.match(line)
        if m:
            name = m.group(1)
            t = infer_expr_type(m.group(2))
            add(name, i, line.find(name), 14, f"imut {t}")
            continue

        m = FOR_RE.match(line)
        if m:
            n1 = m.group(1)
            n2 = m.group(2)
            add(n1, i, line.find(n1), 6, "loop var")
            if n2:
                add(n2, i, line.find(n2), 6, "loop var")
            continue

        m = ASSIGN_RE.match(line)
        if m:
            name = m.group(1)
            t = infer_expr_type(m.group(2))
            add(name, i, line.find(name), 6, t)
            continue

    return defs


def parse_function_params(text: str) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for line in text.splitlines():
        m = FUNC_RE.match(line)
        if not m:
            continue
        name = m.group(1)
        params_text = (m.group(2) or "").strip()
        out[name] = parse_param_names(params_text)
    return out


def def_to_location(uri: str, d: SymbolDef) -> dict[str, Any]:
    return {
        "uri": uri,
        "range": {
            "start": {"line": d.line, "character": d.col},
            "end": {"line": d.line, "character": d.col + len(d.name)},
        },
    }


def pick_definition(symbols: list[SymbolDef], line: int) -> SymbolDef:
    before = [d for d in symbols if d.line <= line]
    if before:
        return before[-1]
    return symbols[0]


def find_ajasendiri_binary() -> str:
    env = os.getenv("AJA_BIN")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    local = os.path.join(root, "ajasendiri")
    if os.path.isfile(local) and os.access(local, os.X_OK):
        return local
    return "ajasendiri"


def parse_first_error(stderr: str) -> tuple[int, int, str]:
    msg = stderr.strip().splitlines()[0] if stderr.strip() else "check failed"
    m = re.search(r"line\s+(\d+)(?::(\d+))?", stderr)
    if not m:
        return (0, 0, msg)
    line = int(m.group(1)) - 1
    col = int(m.group(2)) - 1 if m.group(2) else 0
    return (max(line, 0), max(col, 0), msg)


def build_diagnostics(doc: Document, aja_bin: str) -> list[dict[str, Any]]:
    path = uri_to_path(doc.uri)
    tmp_path = None
    target = None
    if path and path.endswith(".aja"):
        target = path
    else:
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".aja", mode="w", encoding="utf-8")
        tmp.write(doc.text)
        tmp.flush()
        tmp.close()
        tmp_path = tmp.name
        target = tmp_path

    try:
        proc = subprocess.run(
            [aja_bin, "check", target],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5,
        )
        if proc.returncode == 0:
            return []
        line, col, msg = parse_first_error(proc.stderr)
        return [
            {
                "range": {
                    "start": {"line": line, "character": col},
                    "end": {"line": line, "character": col + 1},
                },
                "severity": 1,
                "source": "ajasendiri",
                "message": msg,
            }
        ]
    except Exception as exc:
        return [
            {
                "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
                "severity": 1,
                "source": "ajasendiri-lsp",
                "message": f"failed to run check: {exc}",
            }
        ]
    finally:
        if tmp_path and os.path.exists(tmp_path):
            try:
                os.unlink(tmp_path)
            except OSError:
                pass


def completion_items(doc: Document, line: int, char: int) -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    seen_labels: set[str] = set()

    lines = doc.text.splitlines()
    if 0 <= line < len(lines):
        prefix = lines[line][: max(char, 0)]
        ctx = find_call_context(prefix)
        if ctx is not None:
            func_name, arg_prefix = ctx
            already_named = set(re.findall(r"\b([A-Za-z_]\w*)\s*=", arg_prefix))

            candidate_param_lists: list[list[str]] = []
            bare = func_name.split(".")[-1]
            if bare in doc.func_params_by_name:
                candidate_param_lists.append(doc.func_params_by_name[bare])
            if func_name in BUILTIN_CALL_PARAMS:
                candidate_param_lists.append(BUILTIN_CALL_PARAMS[func_name])

            for plist in candidate_param_lists:
                for pname in plist:
                    if pname in already_named or pname in seen_labels:
                        continue
                    items.append(
                        {
                            "label": pname,
                            "kind": 5,
                            "detail": "named argument",
                            "insertText": f"{pname} = ",
                        }
                    )
                    seen_labels.add(pname)

    for kw in KEYWORDS:
        if kw not in seen_labels:
            items.append({"label": kw, "kind": 14})
            seen_labels.add(kw)
    for name, defs in sorted(doc.defs_by_name.items()):
        if not defs:
            continue
        if name in seen_labels:
            continue
        items.append({"label": name, "kind": defs[0].kind, "detail": defs[0].type_info})
        seen_labels.add(name)
    return items


def split_call_args(arg_prefix: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth_paren = 0
    depth_bracket = 0
    depth_brace = 0
    in_string = False
    quote = ""
    escaped = False

    for i, ch in enumerate(arg_prefix):
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                in_string = False
            continue
        if ch in ('"', "'"):
            in_string = True
            quote = ch
            continue
        if ch == "(":
            depth_paren += 1
            continue
        if ch == ")":
            depth_paren = max(depth_paren - 1, 0)
            continue
        if ch == "[":
            depth_bracket += 1
            continue
        if ch == "]":
            depth_bracket = max(depth_bracket - 1, 0)
            continue
        if ch == "{":
            depth_brace += 1
            continue
        if ch == "}":
            depth_brace = max(depth_brace - 1, 0)
            continue
        if ch == "," and depth_paren == 0 and depth_bracket == 0 and depth_brace == 0:
            parts.append(arg_prefix[start:i])
            start = i + 1
    parts.append(arg_prefix[start:])
    return parts


def active_param_index(param_names: list[str], arg_prefix: str) -> int:
    if not param_names:
        return 0

    parts = split_call_args(arg_prefix)
    if not parts:
        return 0

    current = parts[-1].strip()
    m = re.match(r"^([A-Za-z_]\w*)\s*=", current)
    if m:
        pname = m.group(1)
        if pname in param_names:
            return param_names.index(pname)

    pos_count = 0
    for part in parts[:-1]:
        s = part.strip()
        if not s:
            continue
        if re.match(r"^[A-Za-z_]\w*\s*=", s):
            continue
        pos_count += 1

    if pos_count >= len(param_names):
        return len(param_names) - 1
    return pos_count


def signature_help(doc: Document, line: int, char: int) -> dict[str, Any] | None:
    lines = doc.text.splitlines()
    if line < 0 or line >= len(lines):
        return None
    prefix = lines[line][: max(char, 0)]
    ctx = find_call_context(prefix)
    if ctx is None:
        return None

    func_name, arg_prefix = ctx
    bare = func_name.split(".")[-1]

    params: list[str] | None = None
    label: str | None = None

    if bare in doc.func_params_by_name:
        params = doc.func_params_by_name[bare]
        label = f"{bare}({', '.join(params)})"
    elif func_name in BUILTIN_CALL_PARAMS:
        params = BUILTIN_CALL_PARAMS[func_name]
        label = BUILTIN_SIGNATURE_LABELS.get(func_name, f"{func_name}({', '.join(params)})")

    if params is None or label is None:
        return None

    return {
        "signatures": [
            {
                "label": label,
                "parameters": [{"label": p} for p in params],
            }
        ],
        "activeSignature": 0,
        "activeParameter": active_param_index(params, arg_prefix),
    }


def main() -> int:
    rpc = JsonRpc()
    aja_bin = find_ajasendiri_binary()
    docs: dict[str, Document] = {}
    shutdown_requested = False

    while True:
        msg = rpc.read_message()
        if msg is None:
            break

        method = msg.get("method")
        req_id = msg.get("id")
        params = msg.get("params", {})

        if method == "initialize":
            rpc.respond(
                req_id,
                {
                    "capabilities": {
                        "textDocumentSync": 1,
                        "definitionProvider": True,
                        "hoverProvider": True,
                        "completionProvider": {"resolveProvider": False},
                        "signatureHelpProvider": {"triggerCharacters": ["(", ","]},
                    },
                    "serverInfo": {"name": "ajasendiri-lsp", "version": "0.1.0"},
                },
            )
            continue

        if method == "initialized":
            continue

        if method == "shutdown":
            shutdown_requested = True
            rpc.respond(req_id, None)
            continue

        if method == "exit":
            return 0 if shutdown_requested else 1

        if method == "textDocument/didOpen":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            text = td.get("text", "")
            if uri:
                doc = Document(
                    uri=uri,
                    text=text,
                    defs_by_name=parse_symbols(text),
                    func_params_by_name=parse_function_params(text),
                )
                docs[uri] = doc
                rpc.notify("textDocument/publishDiagnostics", {"uri": uri, "diagnostics": build_diagnostics(doc, aja_bin)})
            continue

        if method == "textDocument/didChange":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            changes = params.get("contentChanges", [])
            if uri and changes:
                text = changes[-1].get("text", "")
                doc = Document(
                    uri=uri,
                    text=text,
                    defs_by_name=parse_symbols(text),
                    func_params_by_name=parse_function_params(text),
                )
                docs[uri] = doc
                rpc.notify("textDocument/publishDiagnostics", {"uri": uri, "diagnostics": build_diagnostics(doc, aja_bin)})
            continue

        if method == "textDocument/didClose":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            if uri and uri in docs:
                docs.pop(uri, None)
                rpc.notify("textDocument/publishDiagnostics", {"uri": uri, "diagnostics": []})
            continue

        if method == "textDocument/definition":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            pos = params.get("position", {})
            doc = docs.get(uri or "")
            if not doc:
                rpc.respond(req_id, None)
                continue
            line = int(pos.get("line", 0))
            ch = int(pos.get("character", 0))
            word = get_word_at(doc.text, line, ch)
            if not word or word not in doc.defs_by_name:
                rpc.respond(req_id, None)
                continue
            sym = pick_definition(doc.defs_by_name[word], line)
            rpc.respond(req_id, def_to_location(uri, sym))
            continue

        if method == "textDocument/hover":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            pos = params.get("position", {})
            doc = docs.get(uri or "")
            if not doc:
                rpc.respond(req_id, None)
                continue
            line = int(pos.get("line", 0))
            ch = int(pos.get("character", 0))
            word = get_word_at(doc.text, line, ch)
            if not word or word not in doc.defs_by_name:
                rpc.respond(req_id, None)
                continue
            sym = pick_definition(doc.defs_by_name[word], line)
            rpc.respond(req_id, {"contents": {"kind": "plaintext", "value": f"{word}: {sym.type_info}"}})
            continue

        if method == "textDocument/completion":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            pos = params.get("position", {})
            doc = docs.get(uri or "")
            if not doc:
                rpc.respond(req_id, {"isIncomplete": False, "items": [{"label": k, "kind": 14} for k in KEYWORDS]})
                continue
            line = int(pos.get("line", 0))
            ch = int(pos.get("character", 0))
            rpc.respond(req_id, {"isIncomplete": False, "items": completion_items(doc, line, ch)})
            continue

        if method == "textDocument/signatureHelp":
            td = params.get("textDocument", {})
            uri = td.get("uri")
            pos = params.get("position", {})
            doc = docs.get(uri or "")
            if not doc:
                rpc.respond(req_id, None)
                continue
            line = int(pos.get("line", 0))
            ch = int(pos.get("character", 0))
            rpc.respond(req_id, signature_help(doc, line, ch))
            continue

        if req_id is not None:
            rpc.respond_error(req_id, -32601, f"method not found: {method}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
