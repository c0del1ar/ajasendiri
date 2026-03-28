# AjaSendiri v0.1.0

<p align="center">
  <img src="assts/Gemini_Generated_Image_51sgrc51sgrc51sg.png" alt="AjaSendiri logo" width="420" />
</p>

Ajasendiri is a high-level interpreted programming language written in C, built for people who crave quick results, beautiful code, and a mind free from “syntactic noise.” Whether you’re a beginner or a seasoned dev, Ajasendiri makes it fun and effortless to turn your ideas into code that just works.

"Aja Sendiri" meant "Your Self", stand for your freedom. The goal is simple: keep syntax readable, keep types strict, and keep day-to-day coding practical.

---

## What You Get

- Python-style indentation blocks
- Strict runtime typing (first assignment locks type)
- `fuc` functions with typed params and return types
- First-class functions, lambda-lite, and multi-return values
- `if/elif/else`, `match/case/default`, `for`, `while`, `do-while`
- List/map types, container methods, and comprehensions
- Interfaces (Go-style implicit implementation)
- Import/export with alias and selective import forms
- Concurrency with `kostroutine`, `chan`, `select`, `waitAll`
- Built-in modules: `math`, `time`, `json`, `fs`, `http`, `rand`, `os`, `path`
- Package/dependency tooling via `mmk` (including local registry flow)
- REPL, formatter, checker, debugger, test runner, and LSP helper

---

## Why AjaSendiri

AjaSendiri is for people who want:

- clear code structure,
- early type errors,
- and a lightweight toolchain.

---

## Philosophy

> "When you find yourself lost in fierce competition, tighten your belt first."
> — R

In practice, this means the language should help you write code quickly without hiding important behavior.

---

## Getting Started

Prerequisites:

- C compiler (`cc`/`gcc`/`clang`)
- `make`
- `python3` (for LSP helper)

Build:

```bash
make
```

Run tests:

```bash
make test
```

---

## Quick Start

Run a file:

```bash
./ajasendiri examples/ok.aja
```

Type-check without running:

```bash
./ajasendiri check examples/ok.aja
```

Example:

```aja
fuc add(a: int, b: int) -> int:
    return a + b

total = add(3, 4)
print(total)
```

---

## Essential Commands

```bash
./ajasendiri                        # REPL
./ajasendiri file.aja               # Run file
./ajasendiri check file.aja         # Type-check only
./ajasendiri test                   # Run test suite
./ajasendiri fmt path/to/file.aja   # Format code
./ajasendiri debug file.aja         # Debug mode
./ajasendiri venv .venv             # Create virtual environment
python3 tools/ajasendiri_lsp.py     # Language server
```

---

## VS Code Syntax Highlighting

Folder: `tools/vscode-ajasendiri`

1. Open that folder in VS Code.
2. Press `F5`.
3. Open a `.aja` file in the Extension Development Host.

---

## Documentation

- Developer overview: `DEV.rst`
- Contributing guide: `CONTRIBUTING.md`
- Docs index: `docs/index.rst`
- Language reference: `docs/language.rst`
- Stdlib reference: `docs/stdlib.rst`
- Concurrency: `docs/concurrency.rst`
- Tooling: `docs/tooling.rst`
- Architecture: `docs/architecture.rst`
