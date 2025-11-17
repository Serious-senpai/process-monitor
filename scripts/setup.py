#!/usr/bin/python

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).parent.parent.resolve()

# Create VSCode settings
VSCODE = ROOT / ".vscode"
VSCODE.mkdir(parents=True, exist_ok=True)

VSCODE_SETTINGS = VSCODE / "settings.json"
with VSCODE_SETTINGS.open("w", encoding="utf-8") as writer:
    settings = {
        "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json",
        "C_Cpp.files.exclude": {},
        "editor.formatOnSave": True,
        "editor.tabSize": 4,
        "extensions.ignoreRecommendations": False,
        "files.insertFinalNewline": True,
        "git.autorefresh": True,
        "git.enabled": True,
        "rust-analyzer.check.overrideCommand": ["cargo", "check", "--message-format=json"],
        "rust-analyzer.linkedProjects": [
            "${workspaceFolder}/common-ffi/Cargo.toml",
        ],
        "rust-analyzer.rustfmt.extraArgs": ["+nightly"],
    }

    if sys.platform == "win32":
        settings["C_Cpp.files.exclude"]["**/linux/**"] = True
        settings["rust-analyzer.linkedProjects"].append("${workspaceFolder}/windows-listener/Cargo.toml")

    elif sys.platform == "linux":
        settings["C_Cpp.files.exclude"]["**/win32/**"] = True
        settings["rust-analyzer.linkedProjects"].append("${workspaceFolder}/linux-listener/Cargo.toml")

    else:
        raise RuntimeError(f"Unsupported platform {sys.platform!r}")

    json.dump(settings, writer, indent=4)

# Modify environment variables
if sys.platform == "win32":
    run = ROOT / "run.bat"

    with run.open("w", encoding="utf-8") as writer:
        writer.write("@echo off\n")
        writer.write(f"cd /d {ROOT}\n")

        writer.write(f"set LIBCLANG_PATH=")
        writer.write(str(ROOT / "extern" / "clang-llvm-21.1.3"))
        writer.write("\n")

        writer.write("cmd\n")
