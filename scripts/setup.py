#!/usr/bin/python

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).parent.parent.resolve()

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
        "rust-analyzer.linkedProjects": [],
        "rust-analyzer.rustfmt.extraArgs": [
            "+nightly",
        ],
    }

    if sys.platform == "win32":
        settings["C_Cpp.files.exclude"]["**/linux/**"] = True
        settings["rust-analyzer.linkedProjects"].append("${workspaceFolder}/windows-listener")

    elif sys.platform == "linux":
        settings["C_Cpp.files.exclude"]["**/win32/**"] = True
        settings["rust-analyzer.linkedProjects"].append("${workspaceFolder}/linux-listener")

    else:
        raise RuntimeError(f"Unsupported platform {sys.platform!r}")

    json.dump(settings, writer, indent=4)
