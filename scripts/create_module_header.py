#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path


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


def render_header(module_pascal: str, module_upper: str, includes: list[str]) -> str:
    header_name = f"Cflat{module_pascal}.h"
    guard = f"CFLAT_{module_upper}_H"
    impl_macro = f"CFLAT_{module_upper}_IMPLEMENTATION"
    no_alias_macro = f"CFLAT_{module_upper}_NO_ALIAS"

    include_lines = "\n".join(f'#include "{inc}"' for inc in includes)
    return f"""#ifndef {guard}
#define {guard}

{include_lines}

/* Public API */
/* CFLAT_DEF void cflat_{module_pascal.lower()}_example(void); */

#if defined(CFLAT_IMPLEMENTATION)
#define {impl_macro}
#endif

#endif //{guard}

#if defined({impl_macro})

/* Implementation */
/* void cflat_{module_pascal.lower()}_example(void) {{ }} */

#endif // {impl_macro}
#undef {impl_macro}

#if !defined({no_alias_macro})

/* Unnamespaced aliases */
/* #define {module_pascal.lower()}_example cflat_{module_pascal.lower()}_example */

#endif // {no_alias_macro}
"""


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a new Cflat module header template under src/."
    )
    parser.add_argument("name", help="Module name (e.g. audio-queue, AudioQueue, CflatAudioQueue)")
    parser.add_argument(
        "--output-dir",
        default="src",
        help="Directory where the header will be created (default: src)",
    )
    parser.add_argument(
        "--include",
        action="append",
        default=["CflatCore.h"],
        help='Header include to add (repeatable). Default: "CflatCore.h"',
    )
    parser.add_argument("--force", action="store_true", help="Overwrite output if it already exists")
    args = parser.parse_args()

    raw_name = args.name
    if raw_name.startswith("Cflat"):
        raw_name = raw_name[len("Cflat") :]

    module_pascal, module_upper = module_parts(raw_name)
    filename = f"Cflat{module_pascal}.h"
    output_path = Path(args.output_dir) / filename

    if output_path.exists() and not args.force:
        raise SystemExit(f"{output_path} already exists. Use --force to overwrite.")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    content = render_header(module_pascal, module_upper, args.include)
    output_path.write_text(content, encoding="utf-8")

    print(f"Created {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
