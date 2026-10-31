#!/usr/bin/env python3
"""Writes vendor/innospect-0.2.0: the published crate with patches/ applied.

    python3 prepare.py

Cargo.toml's [patch] section points at that folder, so run this before the
first build and after any change to patches/. The crate comes from Cargo's
own download cache when it is there, otherwise from crates.io, and is
refused unless its sha256 is SHA256 below, the one crates.io publishes for
innospect 0.2.0 (Cargo.lock cannot pin it: a [patch] path dependency has no
checksum there). The patches apply in name order with `patch -p1`, with no
fuzz and no questions; one that does not apply exactly stops the script.
"""
import hashlib
import io
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
NAME = "innospect"
VERSION = "0.2.0"
SHA256 = "eac77c440f9d6c33c784b6fa4afcd79fb59863bf51a268cc3b21aa3e9432ecac"
URL = "https://static.crates.io/crates/%s/%s-%s.crate" % (NAME, NAME, VERSION)
TOP = "%s-%s" % (NAME, VERSION)


def fail(message):
    print("prepare.py: " + message, file=sys.stderr)
    sys.exit(1)


def cached_crate():
    """The .crate from Cargo's download cache, if one is there."""
    cargo = os.environ.get("CARGO_HOME") or str(pathlib.Path.home() / ".cargo")
    home = pathlib.Path(cargo) / "registry" / "cache"
    for path in sorted(home.glob("*/%s.crate" % TOP)):
        return path.read_bytes()
    return None


def fetch():
    data = cached_crate()
    if data is None or hashlib.sha256(data).hexdigest() != SHA256:
        with urllib.request.urlopen(URL, timeout=60) as response:
            data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SHA256:
        fail("%s has sha256 %s, expected %s" % (URL, digest, SHA256))
    return data


def unpack(data, vendor):
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        members = tar.getmembers()
        for member in members:
            parts = pathlib.PurePosixPath(member.name).parts
            if (not parts or parts[0] != TOP or ".." in parts or member.name.startswith("/")
                    or not (member.isfile() or member.isdir())):
                fail("unexpected entry in the crate: %r" % member.name)
        if hasattr(tarfile, "data_filter"):  # Python 3.12, and backports
            tar.extractall(vendor, members=members, filter="data")
        else:
            tar.extractall(vendor, members=members)


def main():
    vendor = HERE / "vendor"
    dest = vendor / TOP
    data = fetch()
    if dest.exists():
        shutil.rmtree(dest)
    vendor.mkdir(exist_ok=True)
    unpack(data, vendor)
    patches = sorted((HERE / "patches").glob("*.patch"))
    if not patches:
        fail("no patches in %s" % (HERE / "patches"))
    for patch in patches:
        result = subprocess.run(["patch", "-p1", "-t", "-F", "0", "-i", str(patch)], cwd=dest,
                                capture_output=True, text=True)
        if result.returncode != 0:
            fail("%s does not apply:\n%s%s" % (patch.name, result.stdout, result.stderr))
    print("%s: innospect %s, sha256 checked, %d patches applied" % (dest, VERSION, len(patches)))


if __name__ == "__main__":
    main()
