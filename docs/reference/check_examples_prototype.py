# Prototype used to validate the seed cards and recipes (2026-11-26). M1 replaces it with
# cxxman-gen + the CMake snippet target. Run from repo root: python docs/reference/check_examples_prototype.py
import tomllib, pathlib, subprocess, sys
order = {"C++98":98,"C++11":11,"C++14":14,"C++17":17,"C++20":20,"C++23":23}
def rank(s): return {98:0,11:1,14:2,17:3,20:4,23:5}[order[s]]
fails = 0
files = sorted(pathlib.Path("data/entries").rglob("*.toml")) + sorted(pathlib.Path("data/recipes").rglob("*.toml"))
for p in files:
    d = tomllib.loads(p.read_text())
    if "variant" in d:
        d["example"] = [{"code": v["code"], "std": v.get("since","C++17")} for v in d["variant"]]
        d.setdefault("since", "C++98")
    t = d.get("test", {})
    for i, ex in enumerate(d["example"]):
        std = max(["C++17", d["since"], ex.get("std","C++17")], key=rank)
        src = "".join(f"#include <{h}>\n" for h in t.get("includes", []))
        body = "\n".join(e["code"] for e in d["example"][:i]) + "\n" if t.get("chain") else ""
        src += t.get("global","") + "\nint main() {\n" + t.get("prelude","") + "\n" + body + ex["code"] + "\n}\n"
        for cc in ["g++", "clang++"]:
            flag = "-std=c++" + std[3:]
            r = subprocess.run([cc, flag, "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-fsyntax-only", "-x", "c++", "-"], input=src, text=True, capture_output=True)
            ok = r.returncode == 0
            if not ok:
                fails += 1
                print(f"FAIL {p.name}#{i} {cc} {flag}\n{r.stderr[:800]}")
        print(f"{'ok ' if True else ''}{d['id']}#{i} [{std}]")
print("fails:", fails); sys.exit(1 if fails else 0)
