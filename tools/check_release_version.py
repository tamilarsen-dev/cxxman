"""Fail a release build when its pushed tag and package versions disagree."""

from __future__ import annotations

import ast
import os
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    with (ROOT / "pyproject.toml").open("rb") as project_file:
        project_version = tomllib.load(project_file)["project"]["version"]
    package_tree = ast.parse((ROOT / "src/cxxman/__init__.py").read_text(encoding="utf-8"))
    version_assignment = next(
        node
        for node in package_tree.body
        if isinstance(node, ast.Assign)
        and any(isinstance(target, ast.Name) and target.id == "__version__" for target in node.targets)
    )
    package_version = ast.literal_eval(version_assignment.value)
    tag = os.environ.get("GITHUB_REF_NAME", "")
    tagged_version = tag.removeprefix("v")
    if not tag.startswith("v") or tagged_version != project_version or tagged_version != package_version:
        raise SystemExit(
            "Release tag, pyproject.toml version, and cxxman.__version__ must match "
            f"(tag={tag!r}, project={project_version!r}, package={package_version!r})."
        )
    print(f"Release version verified: {tagged_version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
