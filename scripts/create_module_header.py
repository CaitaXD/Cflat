#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

_doc_path = str(Path(__file__).resolve().parent.parent / "doc")
if _doc_path not in sys.path:
    sys.path.insert(0, _doc_path)
from cflatdoc.model import Namespace


def split_words(name: str) -> list[str]:
    cleaned = re.sub(r"[^A-Za-z0-9]+", " ", name).strip()
    if not cleaned:
        return []
    chunks = cleaned.split()
    words: list[str] = []
    for chunk in chunks:
        words.extend(
            part
            for part in re.findall(r"[A-Z]+(?=[A-Z][a-z]|[0-9]|$)|[A-Z]?[a-z]+|[0-9]+", chunk)
            if part
        )
    return words


def module_parts(raw_name: str) -> tuple[str, str]:
    words = split_words(raw_name)
    if not words:
        raise ValueError("module name must contain letters or digits")

    pascal = "".join(word[:1].upper() + word[1:].lower() for word in words)
    upper_snake = "_".join(word.upper() for word in words)
    return pascal, upper_snake


def render_header(module_pascal: str, module_upper: str, includes: list[str],
                  ns: Namespace | None = None) -> str:
    if ns is None:
        ns = Namespace("cflat")
    header_name = f"{ns.pascal}{module_pascal}.h"
    guard = f"{ns.upper}{module_upper}_H"
    impl_macro = f"{ns.upper}{module_upper}_IMPLEMENTATION"
    no_alias_macro = f"{ns.upper}{module_upper}_NO_ALIAS"

    include_lines = "\n".join(f'#include "{inc}"' for inc in includes)
    return f"""#ifndef {guard}
#define {guard}

{include_lines}

/* Public API */
/* {ns.upper}DEF void {ns.snake}{module_pascal.lower()}_example(void); */

#if defined({ns.upper}IMPLEMENTATION)
#define {impl_macro}
#endif

#endif //{guard}

#if defined({impl_macro})

/* Implementation */
/* void {ns.snake}{module_pascal.lower()}_example(void) {{ }} */

#endif // {impl_macro}
#undef {impl_macro}

#if !defined({no_alias_macro})

/* Unnamespaced aliases */
/* #define {module_pascal.lower()}_example {ns.snake}{module_pascal.lower()}_example */

#endif // {no_alias_macro}
"""


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def main() -> int:
    repo = _repo_root()
    ns = Namespace.infer(repo)
    parser = argparse.ArgumentParser(
        description=f"Create a new {ns.pascal} module header template under src/."
    )
    parser.add_argument("name", help=f"Module name (e.g. audio-queue, AudioQueue, {ns.pascal}AudioQueue)")
    parser.add_argument(
        "--output-dir",
        default="src",
        help="Directory where the header will be created (default: src)",
    )
    parser.add_argument(
        "--include",
        action="append",
        default=[f"{ns.pascal}Core.h"],
        help=f'Header include to add (repeatable). Default: "{ns.pascal}Core.h"',
    )
    parser.add_argument("--force", action="store_true", help="Overwrite output if it already exists")
    parser.add_argument("--namespace", default=None,
                        help=f"Library namespace (default: inferred from repo root: '{ns.prefix}')")
    args = parser.parse_args()

    if args.namespace:
        ns = Namespace(args.namespace)

    raw_name = args.name
    if raw_name.startswith(ns.pascal):
        raw_name = raw_name[len(ns.pascal):]

    module_pascal, module_upper = module_parts(raw_name)
    filename = f"{ns.pascal}{module_pascal}.h"
    output_path = Path(args.output_dir) / filename

    if output_path.exists() and not args.force:
        raise SystemExit(f"{output_path} already exists. Use --force to overwrite.")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    content = render_header(module_pascal, module_upper, args.include, ns)
    output_path.write_text(content, encoding="utf-8")

    print(f"Created {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
