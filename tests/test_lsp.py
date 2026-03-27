#!/usr/bin/env python3
import json
import os
import subprocess
import sys
from typing import Any


def encode_msg(obj: dict[str, Any]) -> bytes:
    body = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body


def decode_stream(data: bytes) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    i = 0
    n = len(data)
    while i < n:
        header_end = data.find(b"\r\n\r\n", i)
        if header_end < 0:
            break
        headers = data[i:header_end].decode("utf-8", errors="replace").split("\r\n")
        i = header_end + 4
        content_len = None
        for h in headers:
            if ":" not in h:
                continue
            k, v = h.split(":", 1)
            if k.strip().lower() == "content-length":
                content_len = int(v.strip())
                break
        if content_len is None or i + content_len > n:
            break
        body = data[i : i + content_len]
        i += content_len
        out.append(json.loads(body.decode("utf-8")))
    return out


def fail(msg: str) -> int:
    print(f"[FAIL] {msg}")
    return 1


def main() -> int:
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    server = os.path.join(root, "tools", "ajasendiri_lsp.py")
    aja_bin = os.path.join(root, "ajasendiri")
    if not os.path.isfile(server):
        return fail(f"missing server script: {server}")
    if not os.path.isfile(aja_bin):
        return fail(f"missing ajasendiri binary: {aja_bin}")

    uri = "file:///tmp/ajasendiri_lsp_test.aja"
    valid_text = (
        "fuc add(a: int, b: int) -> int:\n"
        "    return a + b\n"
        "x = 1\n"
        "y = add(x, 2)\n"
    )
    invalid_text = "x = 1\nx = \"oops\"\n"

    msgs = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        {"jsonrpc": "2.0", "method": "initialized", "params": {}},
        {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {"uri": uri, "languageId": "ajasendiri", "version": 1, "text": valid_text},
            },
        },
        {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "textDocument/definition",
            "params": {"textDocument": {"uri": uri}, "position": {"line": 3, "character": 5}},
        },
        {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "textDocument/hover",
            "params": {"textDocument": {"uri": uri}, "position": {"line": 3, "character": 8}},
        },
        {
            "jsonrpc": "2.0",
            "id": 4,
            "method": "textDocument/completion",
            "params": {"textDocument": {"uri": uri}, "position": {"line": 3, "character": 0}},
        },
        {
            "jsonrpc": "2.0",
            "id": 5,
            "method": "textDocument/completion",
            "params": {"textDocument": {"uri": uri}, "position": {"line": 3, "character": 8}},
        },
        {
            "jsonrpc": "2.0",
            "id": 6,
            "method": "textDocument/signatureHelp",
            "params": {"textDocument": {"uri": uri}, "position": {"line": 3, "character": 10}},
        },
        {
            "jsonrpc": "2.0",
            "method": "textDocument/didChange",
            "params": {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": invalid_text}]},
        },
        {"jsonrpc": "2.0", "id": 9, "method": "shutdown", "params": {}},
        {"jsonrpc": "2.0", "method": "exit", "params": {}},
    ]
    payload = b"".join(encode_msg(m) for m in msgs)

    env = os.environ.copy()
    env["AJA_BIN"] = aja_bin
    proc = subprocess.run(
        [sys.executable, server],
        input=payload,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        cwd=root,
        timeout=10,
    )
    if proc.returncode != 0:
        return fail(f"lsp server exited with code {proc.returncode}; stderr: {proc.stderr.decode('utf-8', errors='replace')}")

    out_msgs = decode_stream(proc.stdout)
    by_id: dict[int, dict[str, Any]] = {}
    diagnostics: list[dict[str, Any]] = []
    for m in out_msgs:
        if "id" in m and isinstance(m["id"], int):
            by_id[m["id"]] = m
        if m.get("method") == "textDocument/publishDiagnostics":
            diagnostics.append(m)

    init = by_id.get(1)
    if not init or "result" not in init:
        return fail("missing initialize response")
    caps = init["result"].get("capabilities", {})
    if not caps.get("definitionProvider"):
        return fail("initialize capabilities missing definitionProvider")
    if not caps.get("hoverProvider"):
        return fail("initialize capabilities missing hoverProvider")
    if "completionProvider" not in caps:
        return fail("initialize capabilities missing completionProvider")
    if "signatureHelpProvider" not in caps:
        return fail("initialize capabilities missing signatureHelpProvider")

    dresp = by_id.get(2, {}).get("result")
    if not isinstance(dresp, dict):
        return fail("definition response missing")
    dline = dresp.get("range", {}).get("start", {}).get("line")
    if dline != 0:
        return fail(f"definition line mismatch: expected 0, got {dline}")

    hresp = by_id.get(3, {}).get("result")
    htxt = ""
    if isinstance(hresp, dict):
        htxt = hresp.get("contents", {}).get("value", "")
    if "int" not in htxt:
        return fail(f"hover missing inferred type info, got: {htxt!r}")

    cresp = by_id.get(4, {}).get("result", {})
    labels = {item.get("label") for item in cresp.get("items", []) if isinstance(item, dict)}
    if "add" not in labels:
        return fail("completion missing symbol 'add'")
    if "if" not in labels:
        return fail("completion missing keyword 'if'")

    c2resp = by_id.get(5, {}).get("result", {})
    c2labels = {item.get("label") for item in c2resp.get("items", []) if isinstance(item, dict)}
    if "a" not in c2labels or "b" not in c2labels:
        return fail("completion inside call missing function parameter names")

    sresp = by_id.get(6, {}).get("result")
    if not isinstance(sresp, dict):
        return fail("signatureHelp response missing")
    sigs = sresp.get("signatures", [])
    if not sigs:
        return fail("signatureHelp returned no signatures")
    sparams = sigs[0].get("parameters", [])
    if len(sparams) < 2:
        return fail("signatureHelp parameter list too short")
    if sresp.get("activeParameter") != 1:
        return fail(f"signatureHelp activeParameter mismatch: expected 1, got {sresp.get('activeParameter')}")

    if len(diagnostics) < 2:
        return fail("expected diagnostics on open+change notifications")
    last_diag = diagnostics[-1].get("params", {}).get("diagnostics", [])
    if not last_diag:
        return fail("expected non-empty diagnostics after invalid change")

    print("[OK]   lsp_v1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
