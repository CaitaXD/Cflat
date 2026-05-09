#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path


ALIAS_BLOCK_RE = re.compile(
    r"(?ms)"
    r"^(?P<start>[ \t]*#(?:if\s+!defined\s*\(|ifndef\s+)"
    r"(?P<macro>CFLAT_[A-Z0-9_]+_NO_ALIAS)\)?[^\n]*\n)"
    r"(?P<body>.*?)"
    r"(?P<end>^[ \t]*#endif[^\n]*NO_ALIAS[^\n]*\n?)"
)

DEFINE_RE = re.compile(r"(?m)^[ \t]*#\s*define\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\b")

FUNCTION_RE = re.compile(
    r"(?m)^(?!\s*#)\s*"
    r"(?:CFLAT_DEF\s+)?"
    r"(?:[A-Za-z_][\w\s\*\(\)]*?\s+)"
    r"(cflat_[A-Za-z0-9_]+)\s*"
    r"\([^;{}]*\)\s*(?:;|\{)"
)

TYPEDEF_RE = re.compile(r"(?m)^\s*typedef\b[^;]*\b(Cflat[A-Za-z0-9_]+)\s*;")

PUBLIC_DECL_RE = re.compile(
    r"(?m)^(?P<decl>"
    r"(?![ \t])"
    r"(?!#)"
    r"(?!typedef\b)"
    r"(?!.*\bCFLAT_DEF\b)"
    r"(?!.*\bcflat__)"
    r"[^\n;{}]*\bcflat_[A-Za-z0-9_]+\s*\([^;{}]*\)\s*;"
    r")\s*$"
)

IMPL_MACRO_RE = re.compile(r"(?m)^\s*#\s*define\s+(CFLAT_[A-Z0-9_]+)_IMPLEMENTATION\b")
GUARD_MACRO_RE = re.compile(
    r"(?m)^\s*#\s*(?:ifndef|if\s+!defined\s*\()\s*(CFLAT_[A-Z0-9_]+)_H\b"
)

C_CPP_KEYWORDS = {
    "_alignas", "_alignof", "_atomic", "_bool", "_complex", "_generic", "_imaginary",
    "_noreturn", "_static_assert", "_thread_local", "alignas", "alignof", "asm", "auto",
    "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "continue",
    "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for",
    "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
    "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
    "public", "register", "reinterpret_cast", "requires", "restrict", "return", "short",
    "signed", "sizeof", "static", "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "true", "try", "typedef", "typeid", "typeof", "typename", "union",
    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
}


def to_upper_snake(name: str) -> str:
    parts = re.findall(r"[A-Z]+(?=[A-Z][a-z]|[0-9]|$)|[A-Z]?[a-z]+|[0-9]+", name)
    return "_".join(part.upper() for part in parts if part)


def derive_no_alias_macro(text: str, path: Path) -> str | None:
    impl = IMPL_MACRO_RE.search(text)
    if impl:
        return f"{impl.group(1)}_NO_ALIAS"

    guard = GUARD_MACRO_RE.search(text)
    if guard:
        return f"{guard.group(1)}_NO_ALIAS"

    stem = path.stem
    if stem.startswith("Cflat"):
        stem = stem[len("Cflat") :]
    suffix = to_upper_snake(stem)
    if not suffix:
        return None
    return f"CFLAT_{suffix}_NO_ALIAS"


def ensure_alias_block(text: str, path: Path) -> tuple[str, bool]:
    if ALIAS_BLOCK_RE.search(text):
        return text, False

    no_alias_macro = derive_no_alias_macro(text, path)
    if not no_alias_macro:
        return text, False

    block = (
        f"\n#if !defined({no_alias_macro})\n\n"
        f"#endif // {no_alias_macro}\n"
    )
    return text.rstrip("\n") + block, True


def collect_symbols(text: str) -> list[tuple[str, str]]:
    out: set[tuple[str, str]] = set()

    for fn in FUNCTION_RE.findall(text):
        if fn.startswith("cflat__"):
            continue
        alias = fn.removeprefix("cflat_")
        if alias.lower() in C_CPP_KEYWORDS:
            continue
        out.add((alias, fn))

    for typ in TYPEDEF_RE.findall(text):
        alias = typ.removeprefix("Cflat")
        if alias and alias.lower() not in C_CPP_KEYWORDS:
            out.add((alias, typ))

    return sorted(out, key=lambda x: x[0].lower())


def normalize_missing_cflat_def(text: str) -> tuple[str, int]:
    def repl(match: re.Match[str]) -> str:
        decl = match.group("decl")
        if re.search(r"\b(static|inline|extern|typedef)\b", decl):
            return match.group(0)
        return f"CFLAT_DEF {decl.strip()}"

    return PUBLIC_DECL_RE.subn(repl, text)


def inject_aliases(text: str, path: Path) -> tuple[str, int, bool]:
    text, block_created = ensure_alias_block(text, path)
    symbols = collect_symbols(text)
    match = ALIAS_BLOCK_RE.search(text)
    if not match:
        return text, 0, block_created

    body = match.group("body")
    existing_aliases = {lhs for lhs, _ in DEFINE_RE.findall(body)}
    existing_targets = {rhs for _, rhs in DEFINE_RE.findall(body)}

    additions = [
        f"#   define {alias} {target}"
        for alias, target in symbols
        if alias not in existing_aliases and target not in existing_targets
    ]

    text_before = re.sub(r"(?ms)/\*.*?\*/|//[^\n]*", "", text[: match.start()])
    text_after = re.sub(r"(?ms)/\*.*?\*/|//[^\n]*", "", text[match.end() :])
    lines = body.splitlines()
    kept: list[str] = []
    removed = 0
    for line in lines:
        m = DEFINE_RE.match(line.strip())
        if m and m.group(2).startswith(("cflat_", "CFLAT_")):
            target = m.group(2)
            if not re.search(rf"\b{re.escape(target)}\b", text_before + text_after):
                removed += 1
                continue
        kept.append(line)

    if not additions and removed == 0:
        return text, 0, block_created

    new_body = ("\n".join(kept)).rstrip("\n")
    if new_body:
        new_body += "\n"
    new_body += "\n".join(additions)
    if additions:
        new_body += "\n"

    replacement = f"{match.group('start')}{new_body}{match.group('end')}"
    updated = text[: match.start()] + replacement + text[match.end() :]
    return updated, len(additions) + removed, block_created


def normalize_header(text: str, path: Path) -> tuple[str, int, int, bool]:
    updated, def_count = normalize_missing_cflat_def(text)
    updated, alias_count, block_created = inject_aliases(updated, path)
    return updated, def_count, alias_count, block_created


def target_files(paths: list[str]) -> list[Path]:
    repo_root = Path(__file__).resolve().parent.parent
    if paths:
        return [Path(p).resolve() for p in paths]

    src_dir = Path.cwd() / "src"
    if src_dir.is_dir():
        return sorted(src_dir.glob("*.h"))
    return sorted((repo_root / "src").glob("*.h"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Normalize Cflat headers by adding missing CFLAT_DEF on public cflat_* "
            "declarations and injecting missing unnamespaced aliases."
        )
    )
    parser.add_argument("paths", nargs="*", help="Header files to process (defaults to src/*.h)")
    parser.add_argument("--dry-run", action="store_true", help="Show what would change without writing")
    args = parser.parse_args()

    changed_files = 0
    added_defs = 0
    added_aliases = 0
    created_blocks = 0

    for path in target_files(args.paths):
        if not path.exists() or not path.is_file():
            continue

        text = path.read_text(encoding="utf-8")
        updated, defs_count, aliases_count, block_created = normalize_header(text, path)
        if defs_count == 0 and aliases_count == 0 and not block_created:
            continue

        changed_files += 1
        added_defs += defs_count
        added_aliases += aliases_count
        if block_created:
            created_blocks += 1
        print(
            f"{path}: +{defs_count} CFLAT_DEF, {'~' if aliases_count else '='}{aliases_count} aliases, "
            f"{'+1 alias block' if block_created else '+0 alias block'}"
        )

        if not args.dry_run:
            path.write_text(updated, encoding="utf-8")

    if changed_files == 0:
        print("No normalization changes needed.")
    else:
        print(
            f"Updated {changed_files} file(s), inserted "
            f"{added_defs} CFLAT_DEF, {added_aliases} alias(es), "
            f"created {created_blocks} alias block(s)."
        )

    return 1 if args.dry_run and changed_files else 0


if __name__ == "__main__":
    raise SystemExit(main())
