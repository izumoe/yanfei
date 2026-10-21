#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a complete Yanfei Pages site from a firmware package and emulator.

python3 web/make_pages.py build/yanfei.fwsc v1.6.1-y13 build/pages --emulator build/emu
Install build dependencies with: pip install -r web/requirements-pages.txt
"""
import argparse
import hashlib
import html
import json
import re
import shutil
import subprocess
import tarfile
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

import markdown
import make_site

ROOT = Path(__file__).resolve().parents[1]


def source_files():
    """Include tracked and new source files, including the engine working tree."""
    paths = set()
    for base in (ROOT, ROOT / "firmware/engines"):
        result = subprocess.check_output(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=base)
        for name in result.split(b"\0"):
            if not name:
                continue
            path = base / name.decode()
            if path.is_file() and not path.is_symlink():
                paths.add(path)
    return sorted(paths)


def rewrite_links(document, upstream=False):
    def replace(match):
        target = urlsplit(html.unescape(match[1]))
        if target.scheme or target.netloc or not target.path:
            return match[0]
        if upstream:
            return 'href="https://github.com/hugelton/Felucca/blob/main/' + html.escape(match[1], quote=True) + '"'
        if not target.path.endswith(".md"):
            return match[0]
        target = target._replace(path=target.path[:-3] + ".html")
        return 'href="' + html.escape(urlunsplit(target), quote=True) + '"'
    return re.sub(r'href="([^"]+)"', replace, document)


def write_docs(out):
    paths = sorted((ROOT / "docs").glob("*.md"))
    paths += [ROOT / name for name in ("README.md", "BUILDING.md", "CONTRIBUTING.md", "LICENSING.md", "CREDITS.md")]
    for path in paths:
        relative = path.relative_to(ROOT)
        destination = out / relative.with_suffix(".html")
        destination.parent.mkdir(parents=True, exist_ok=True)
        body = markdown.markdown(path.read_text(), extensions=["tables", "fenced_code", "toc"])
        home = "../index.html" if relative.parts[0] == "docs" else "index.html"
        title = html.escape(path.read_text().splitlines()[0].lstrip("# "))
        destination.write_text(
            '<!doctype html><html lang="en"><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<title>{title} — Yanfei</title><style>'
            'body{max-width:900px;margin:32px auto;padding:0 20px;font:16px/1.65 system-ui,sans-serif;color:#222}'
            'a{color:inherit}img{max-width:100%;height:auto}figure{margin:24px 0}figcaption{color:#444;font-size:15px;max-width:60ch}img.screen{image-rendering:pixelated}h2,h3{scroll-margin-top:100px}nav{display:flex;flex-wrap:wrap;gap:8px 20px;position:sticky;top:0;background:#fff;padding:8px 0;border-bottom:1px solid #ddd;z-index:1}pre{overflow:auto;background:#f4f4f4;padding:16px}'
            'table{border-collapse:collapse;display:block;overflow:auto}td,th{border:1px solid #ccc;padding:8px;text-align:left}'
            '</style><nav><a href="' + home + '">Yanfei home</a><a href="' + ('VISUAL_GUIDE.html' if relative.parts[0] == 'docs' else 'docs/VISUAL_GUIDE.html') + '">Visual guide</a><a href="' + ('TRACKER_QUICK_REFERENCE.html' if relative.parts[0] == 'docs' else 'docs/TRACKER_QUICK_REFERENCE.html') + '">Controls</a><a href="' + ('MODE_GUIDE.html' if relative.parts[0] == 'docs' else 'docs/MODE_GUIDE.html') + '">Modes</a><a href="' + ('ENGINE_GUIDE.html' if relative.parts[0] == 'docs' else 'docs/ENGINE_GUIDE.html') + '">Engines</a></nav><main>'
            + rewrite_links(body, path.name == "UPSTREAM_README.md") + '</main></html>')
        shutil.copy2(path, out / relative)
    shutil.copytree(ROOT / "docs/images", out / "docs/images", dirs_exist_ok=True)
    shutil.copy2(ROOT / "LICENSE", out / "LICENSE")
    shutil.copytree(ROOT / "LICENSES", out / "LICENSES", dirs_exist_ok=True)
    shutil.copy2(out / "firmware/LICENSES/index.html", out / "LICENSES/index.html")
    attribution = Path("assets/samples-cc0/ATTRIBUTION.txt")
    (out / attribution).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / attribution, out / attribution)


def main(package, version, out, emulator):
    package, out, emulator = Path(package).resolve(), Path(out).resolve(), Path(emulator).resolve()
    # Pages output belongs under ignored build/: never archive a published tree
    # or put generated site files into the source archive on the next build.
    if not out.is_relative_to(ROOT / "build"):
        raise SystemExit("Pages output must be under this checkout's build/ directory")
    for name in ("index.html", "worklet.js", "felucca.wasm", "fonts/OFL.txt", "fonts/DotGothic16-subset.woff"):
        if not (emulator / name).is_file():
            raise SystemExit(f"emulator missing {name}; run FELUCCA_CORE=1 sh web/emu/build.sh")
    if version not in (emulator / "index.html").read_text():
        raise SystemExit("emulator version differs from requested Pages version")
    make_site.main(package, version, out)
    runtime = out / "webapp/try"
    runtime.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "worklet.js", "felucca.wasm"):
        shutil.copy2(emulator / name, runtime / name)
    shutil.copytree(emulator / "fonts", runtime / "fonts", dirs_exist_ok=True)
    write_docs(out)

    safe_version = re.sub(r"[^A-Za-z0-9.-]", "-", version)
    source = out / "source" / f"yanfei-{safe_version}-source.tar.gz"
    source.parent.mkdir(exist_ok=True)
    files = source_files()
    manifest = {"version": version, "source": "workspace snapshot, including uncommitted changes",
                "firmware_sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
                "emulator_sha256": hashlib.sha256((emulator / "felucca.wasm").read_bytes()).hexdigest(),
                "root_base": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
                "engines_base": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT / "firmware/engines").decode().strip(),
                "files": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
    (source.parent / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with tarfile.open(source, "w:gz", dereference=False) as archive:
        for path in files:
            archive.add(path, arcname=f"yanfei-{safe_version}/{path.relative_to(ROOT)}", recursive=False)
        archive.add(source.parent / "manifest.json", arcname=f"yanfei-{safe_version}/SOURCE_MANIFEST.json")
    firmware = next((out / "firmware").glob("yanfei-*.fwsc"))
    landing = (ROOT / "web/pages_index.html").read_text()
    for marker, value in {"@VERSION@": version, "@FIRMWARE@": str(firmware.relative_to(out)),
                          "@SOURCE@": str(source.relative_to(out))}.items():
        landing = landing.replace(marker, html.escape(value, quote=True))
    (out / "index.html").write_text(landing)
    (out / ".nojekyll").write_text("")
    (out / "SHA256SUMS").write_text("".join(
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(out)}\n"
        for p in sorted(out.rglob("*")) if p.is_file() and p.name != "SHA256SUMS"))
    print(f"pages: {out}; editor, emulator, installer, guide, firmware and {len(files)} source files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package")
    parser.add_argument("version")
    parser.add_argument("out")
    parser.add_argument("--emulator", required=True)
    args = parser.parse_args()
    main(args.package, args.version, args.out, args.emulator)
