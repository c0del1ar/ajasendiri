# AjaSendiri v0.1.0

**Ajasendiri** is a high-level programming language built for people who crave quick results, beautiful code, and a mind free from “syntactic noise.” Whether you’re a beginner or a seasoned dev, Ajasendiri makes it fun and effortless to turn your ideas into code that just works.

---

## What Makes Ajasendiri Special?

- **Readable, Pythonic Syntax:** Indent your code, not your frustration. Clean blocks, zero boilerplate, maximum clarity.
- **Boldly Typed:** Variables pick a side and stick to it (type locks on first assignment), so you catch mistakes early, not late.
- **Functional Freedom:** First-class functions, multi-return, lambdas — pass them, return them, love them. 
- **Effortless Concurrency:** Channels and “kostroutine” coroutines come standard. Want Go-like async and a deterministic `select`? It’s all here.
- **Modular Magic:** Import/export how you want. Aliases, explicit control, sane resolution.
- **Batteries Included:** Math, time, JSON, file & HTTP I/O — all ready out-of-the-box. No endless downloads.
- **Happiness-Driven Tooling:** Formatters, type checkers, and a REPL that simply work — so you can focus where it matters.
- **Minimalist Zen:** The language fades into the background, letting you think about your problem, not the syntax.
- **Just Works:** Blazing-fast C engine, zero external dependencies, friendly to macOS/Linux/Windows. If you have `make` and a C compiler, you can run.

---

## Why Ajasendiri?

Because coding should be:
- Fun, not finicky.
- Powerful, not perplexing.
- Productive, not ponderous.

---

## 💡 Philosophy

> "When you find yourself lost in fierce competition, tighten your belt first."  
> — R

Simplicity isn’t just philosophy. In Ajasendiri, it’s built into every line.

---

## Getting Started

**Prerequisites:**
- C compiler (`cc`/`gcc`/`clang`)
- `make`
- `python3` (for LSP helper)

**Build it:**
```bash
make
```

**Run the tests:**
```bash
make test
```

---

## ⚡ Quick Start

Run your first program:
```bash
./ajasendiri examples/ok.aja
```

Type-check without running:
```bash
./ajasendiri check examples/ok.aja
```

---

## A Taste of Ajasendiri

```aja
fuc add(a: int, b: int) -> int:
    return a + b

total = add(3, 4)
print(total)
```

Yes — that’s really it!

---

## 🔧 Essential Commands

```bash
./ajasendiri                        # Start REPL
./ajasendiri test                   # Run tests
./ajasendiri fmt path/to/file.aja   # Format your code
./ajasendiri repl                   # Interactive shell
./ajasendiri debug file.aja         # Debug mode
./ajasendiri venv .venv             # Create virtual environment
python3 tools/ajasendiri_lsp.py     # Language server
```

---

## 🎨 VS Code Syntax Highlighting

Find it at `tools/vscode-ajasendiri`!

1. Open the folder in VS Code.
2. Press `F5`.
3. Open a `.aja` file in the development window. Enjoy the colors!

---

## 📚 Documentation

- 👩‍💻 **Developer overview:** `DEV.rst`
- 🤝 **Contributing guide:** `CONTRIBUTING.md`
- 🗺 **Docs index:** `docs/index.rst`
- 📝 **Language reference:** `docs/language.rst`
- 📦 **Modules & stdlib:** `docs/stdlib.rst`
- ⚡ **Concurrency guide:** `docs/concurrency.rst`
- 🔧 **Tooling:** `docs/tooling.rst`
- ⚙️ **Implementation notes:** `docs/architecture.rst`

---

Ajasendiri — simplicity in action. TTD;gd
