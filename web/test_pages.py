#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check a generated Pages tree, local links, and corresponding source archive."""
import hashlib
import json
import sys
import tarfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


class Links(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.targets, self.ids = [], set()
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        if tag in ("a", "link") and "href" in attrs:
            self.targets.append(attrs["href"])
        if tag in ("img", "script") and "src" in attrs:
            self.targets.append(attrs["src"])


def main(directory):
    root = Path(directory).resolve()
    assert (root / ".nojekyll").is_file()
    for name in ("index.html", "webapp/editor/index.html", "webapp/installer/index.html",
                 "webapp/try/index.html", "webapp/try/worklet.js", "webapp/try/felucca.wasm",
                 "docs/USER_GUIDE.html", "docs/TRACKER_QUICK_REFERENCE.html"):
        assert (root / name).is_file(), name
    pages = {p.resolve(): Links(p.read_text()) for p in root.rglob("*.html")}
    failures, links = [], 0
    for page, parsed in pages.items():
        for href in parsed.targets:
            url = urlsplit(href)
            if url.scheme or url.netloc:
                continue
            links += 1
            if url.path.startswith("/"):
                failures.append(f"{page.relative_to(root)}: absolute path breaks a project site: {href}")
                continue
            target = (page.parent / unquote(url.path)).resolve() if url.path else page
            if target.is_dir():
                target /= "index.html"
            if not target.is_relative_to(root) or not target.is_file():
                failures.append(f"{page.relative_to(root)}: missing {href}")
            elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:
                failures.append(f"{page.relative_to(root)}: missing anchor {href}")
    assert not failures, "\n".join(failures)
    manifest = json.loads((root / "source/manifest.json").read_text())
    firmware = next((root / "firmware").glob("yanfei-*.fwsc"))
    assert hashlib.sha256(firmware.read_bytes()).hexdigest() == manifest["firmware_sha256"]
    assert hashlib.sha256((root / "webapp/try/felucca.wasm").read_bytes()).hexdigest() == manifest["emulator_sha256"]
    source = next((root / "source").glob("*.tar.gz"))
    with tarfile.open(source) as archive:
        members = archive.getmembers()
        prefix = members[0].name.split("/")[0] + "/"
        for name in ("firmware/src/ui_tracker.c", "firmware/engines/src/eng_psg.c", "build-yanfei.sh", "LICENSE"):
            assert name in manifest["files"], name
        for member in members:
            relative = member.name.removeprefix(prefix)
            assert not member.issym() and not member.islnk()
            assert not set(Path(relative).parts) & {".git", "build", ".."}, relative
        for name, digest in manifest["files"].items():
            assert hashlib.sha256(archive.extractfile(prefix + name).read()).hexdigest() == digest, name
    for line in (root / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name
    print(f"pages: {len(pages)} HTML pages, {links} local links, {len(manifest['files'])} archived source files and all checksums passed")


if __name__ == "__main__":
    main(sys.argv[1])
