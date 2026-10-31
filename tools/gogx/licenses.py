#!/usr/bin/env python3
"""Prints the licence notices for every crate compiled into gogx.

    python3 licenses.py [--target TRIPLE ...] > THIRD-PARTY.txt

The crates are gogx's normal dependencies, followed through Cargo.lock for
each TRIPLE (default: both Mac targets, whose slices make one universal
binary) and joined. Build-time, test-only and procedural-macro crates run
only while compiling, are not in the binary, and are left out. For each
crate it prints its declared licence and every licence, copyright or
notice file it ships. A crate that ships none is an
error, not an omission: its notice would then have to be written by hand.
Run prepare.py first, so the patched innospect is where Cargo.toml says.
"""
import argparse
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
PREFIXES = ("license", "licence", "copying", "notice", "copyright", "authors")


def metadata(target):
    out = subprocess.run(
        ["cargo", "metadata", "--format-version", "1", "--locked", "--filter-platform", target],
        cwd=HERE, check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def is_proc_macro(package):
    return any("proc-macro" in t["kind"] for t in package["targets"])


def shipped(meta):
    """Package ids reachable from gogx through normal dependencies, not
    through a procedural macro."""
    nodes = {n["id"]: n for n in meta["resolve"]["nodes"]}
    packages = {p["id"]: p for p in meta["packages"]}
    root = meta["resolve"]["root"]
    seen, stack = set(), [root]
    while stack:
        pid = stack.pop()
        if pid in seen or is_proc_macro(packages[pid]):
            continue
        seen.add(pid)
        for dep in nodes[pid]["deps"]:
            if any(k["kind"] is None for k in dep["dep_kinds"]):
                stack.append(dep["pkg"])
    seen.discard(root)
    return seen


def licence_files(package):
    folder = pathlib.Path(package["manifest_path"]).parent
    files = sorted(p for p in folder.iterdir()
                   if p.is_file() and p.name.lower().startswith(PREFIXES))
    if package.get("license_file"):
        extra = folder / package["license_file"]
        if extra.is_file() and extra not in files:
            files.append(extra)
    return files


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", action="append",
                        help="a target triple; repeat for several (default: both Mac targets)")
    args = parser.parse_args()
    packages, chosen = {}, set()
    for target in args.target or ["x86_64-apple-darwin", "aarch64-apple-darwin"]:
        meta = metadata(target)
        packages.update({p["id"]: p for p in meta["packages"]})
        chosen |= shipped(meta)
    ids = sorted(chosen, key=lambda i: (packages[i]["name"], packages[i]["version"]))
    out = ["gogx is built with these Rust crates, each under its own licence.",
           "innospect 0.2.0 is modified: Arcanum CE's three fixes are in",
           "tools/gogx/patches/ of the Arcanum CE source, each file marked.", ""]
    missing = []
    for pid in ids:
        package = packages[pid]
        files = licence_files(package)
        if not files:
            missing.append("%s %s" % (package["name"], package["version"]))
            continue
        out += ["=" * 72,
                "%s %s - %s" % (package["name"], package["version"], package.get("license") or "see below"),
                package.get("repository") or "", "=" * 72, ""]
        for path in files:
            out += ["--- %s ---" % path.name, path.read_text(errors="replace").rstrip(), ""]
    if missing:
        print("licenses.py: no licence file in: " + ", ".join(missing), file=sys.stderr)
        sys.exit(1)
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
