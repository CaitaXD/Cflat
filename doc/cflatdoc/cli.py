"""CLI entry point: python -m cflatdoc [...]"""
from __future__ import annotations
import argparse
from pathlib import Path

from .model import Namespace
from .parsing import parse_header, extract_top_level_comments
from .render_roff import render_module_roff, render_index_roff
from .render_html import render_module_html, render_index_html
from .render_json import dump_modules_json


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent

def resolve_paths(args):
    repo = _repo_root()
    src = Path(args.src_dir).resolve() if args.src_dir else (repo / "src")
    out = Path(args.out_dir).resolve() if args.out_dir else (repo / "doc" / "man3")
    html = Path(args.html_dir).resolve() if args.html_dir else (repo / "doc" / "html")
    js = Path(args.json_dir).resolve() if args.json_dir else (repo / "doc" / "json")
    return src, out, html, js


def write_pages(src: Path, out: Path, html: Path, js: Path,
                ns: Namespace | None = None) -> tuple[int, int]:
    if ns is None:
        ns = Namespace.infer(_repo_root())
    headers = sorted(src.glob(f"{ns.pascal}*.h"))
    modules = []
    index_synopsis: list[str] = []

    main_h = src / f"{ns.pascal}.h"
    if main_h.is_file():
        index_synopsis = extract_top_level_comments(main_h.read_text(encoding="utf-8"))

    for h in headers:
        d = parse_header(h, ns)
        if d.module_name == f"{ns.slug}-":
            continue
        if not (d.declarations or d.typedefs or d.macros or d.enums):
            continue
        modules.append(d)

    out.mkdir(parents=True, exist_ok=True)
    html.mkdir(parents=True, exist_ok=True)
    js.mkdir(parents=True, exist_ok=True)

    # Clean stale outputs
    for p in list(out.glob(f"{ns.slug}*.3")) + [out / f"{ns.slug}.3"]:
        if p.exists(): p.unlink()
    for p in list(html.glob(f"{ns.slug}*.html")) + [html / "index.html"]:
        if p.exists(): p.unlink()

    pages = 0
    for m in modules:
        (out / f"{m.module_name}.3").write_text(render_module_roff(m, ns), encoding="utf-8")
        (html / f"{m.module_name}.html").write_text(render_module_html(m, modules, ns), encoding="utf-8")
        pages += 2

    (out / f"{ns.slug}.3").write_text(render_index_roff(modules, index_synopsis, ns), encoding="utf-8")
    (html / "index.html").write_text(render_index_html(modules, index_synopsis, ns), encoding="utf-8")
    (js / "docs.json").write_text(dump_modules_json(modules, index_synopsis), encoding="utf-8")
    pages += 3

    return len(modules), pages


def main() -> int:
    ap = argparse.ArgumentParser(
        prog="cflatdoc",
        description="Generate roff man pages, HTML docs, and JSON from Cflat headers.",
    )
    ap.add_argument("--src-dir", help="Headers directory (default <repo>/src)")
    ap.add_argument("--out-dir", help="Man pages output (default <repo>/man/man3)")
    ap.add_argument("--html-dir", help="HTML output (default <repo>/man/html)")
    ap.add_argument("--json-dir", help="JSON output (default <repo>/man/json)")
    ap.add_argument("--namespace", default=None,
                    help=f"Library namespace (default: inferred from repo root folder name, "
                         f"e.g. '{_repo_root().name.lower()}')")
    args = ap.parse_args()

    src, out, html, js = resolve_paths(args)
    if not src.is_dir():
        raise SystemExit(f"Source directory does not exist: {src}")

    ns = Namespace(args.namespace) if args.namespace else Namespace.infer(_repo_root())
    n, pages = write_pages(src, out, html, js, ns)
    slug = ns.slug
    print(f"{slug}doc: wrote {pages} files from {n} module(s)")
    print(f"  man : {out}")
    print(f"  html: {html}")
    print(f"  json: {js}/docs.json")
    return 0
