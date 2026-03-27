# Ajasendiri Syntax (VS Code)

This extension adds syntax highlighting for `.aja` files.

## Local Use (without publishing)

1. Open VS Code.
2. Open the folder `tools/vscode-ajasendiri`.
3. Press `F5` to run an Extension Development Host.
4. Open any `.aja` file in the new window.

## Optional Packaging

If you want a `.vsix` package:

```bash
cd tools/vscode-ajasendiri
npm i -g @vscode/vsce
vsce package
```

Then install:

```bash
code --install-extension ajasendiri-syntax-0.1.0.vsix
```
