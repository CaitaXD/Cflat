#!/usr/bin/env python3
"""Normalize headers: add missing CFLAT_DEF, inject/clean unnamespaced aliases."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Import Namespace from the doc toolchain
_doc_path = str(Path(__file__).resolve().parent.parent / "doc")
if _doc_path not in sys.path:
    sys.path.insert(0, _doc_path)
from cflatdoc.model import Namespace


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


_ns: Namespace = Namespace("cflat")

ALIAS_BLOCK_RE: re.Pattern
DEFINE_RE: re.Pattern = re.compile(r"(?m)^[ \t]*#\s*define\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\b")
FUNCTION_RE: re.Pattern
TYPEDEF_RE: re.Pattern
PUBLIC_DECL_RE: re.Pattern
IMPL_MACRO_RE: re.Pattern
GUARD_MACRO_RE: re.Pattern


def _compile(ns: Namespace) -> None:
    global _ns, ALIAS_BLOCK_RE, FUNCTION_RE, TYPEDEF_RE, PUBLIC_DECL_RE, IMPL_MACRO_RE, GUARD_MACRO_RE
    _ns = ns
    ALIAS_BLOCK_RE = re.compile(
        rf"(?ms)"
        rf"^(?P<start>[ \t]*#(?:if\s+!defined\s*\(|ifndef\s+)"
        rf"(?P<macro>{_ns.upper}[A-Z0-9_]+_NO_ALIAS)\)?[^\n]*\n)"
        rf"(?P<body>.*?)"
        rf"(?P<end>^[ \t]*#endif[^\n]*NO_ALIAS[^\n]*\n?)"
    )
    FUNCTION_RE = re.compile(
        rf"(?m)^(?!\s*#)\s*"
        rf"(?:{_ns.upper}DEF\s+)?"
        rf"(?:[A-Za-z_][\w\s\*\(\)]*?\s+)"
        rf"({_ns.snake}[A-Za-z0-9_]+)\s*"
        rf"\([^;{{}}]*\)\s*(?:;|\{{)"
    )
    TYPEDEF_RE = re.compile(rf"(?m)^\s*typedef\b[^;]*\b({_ns.pascal}[A-Za-z0-9_]+)\s*;")
    PUBLIC_DECL_RE = re.compile(
        rf"(?m)^(?P<decl>"
        rf"(?![ \t])"
        rf"(?!#)"
        rf"(?!typedef\b)"
        rf"(?!.*\b{_ns.upper}DEF\b)"
        rf"(?!.*\b{_ns.snake}_)"
        rf"[^\n;{{}}]*\b{_ns.snake}[A-Za-z0-9_]+\s*\([^;{{}}]*\)\s*;"
        rf")\s*$"
    )
    IMPL_MACRO_RE = re.compile(rf"(?m)^\s*#\s*define\s+({_ns.upper}[A-Z0-9_]+)_IMPLEMENTATION\b")
    GUARD_MACRO_RE = re.compile(
        rf"(?m)^\s*#\s*(?:ifndef|if\s+!defined\s*\()\s*({_ns.upper}[A-Z0-9_]+)_H\b"
    )


_compile(_ns)


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
    if stem.startswith(_ns.pascal):
        stem = stem[len(_ns.pascal):]
    suffix = to_upper_snake(stem)
    if not suffix:
        return None
    return f"{_ns.upper}{suffix}_NO_ALIAS"


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
        if fn.startswith(f"{_ns.snake}_"):
            continue
        alias = fn.removeprefix(_ns.snake)
        if alias.lower() in C_CPP_KEYWORDS:
            continue
        out.add((alias, fn))

    for typ in TYPEDEF_RE.findall(text):
        alias = typ.removeprefix(_ns.pascal)
        if alias and alias.lower() not in C_CPP_KEYWORDS:
            out.add((alias, typ))

    return sorted(out, key=lambda x: x[0].lower())


def normalize_missing_cflat_def(text: str) -> tuple[str, int]:
    def repl(match: re.Match[str]) -> str:
        decl = match.group("decl")
        if re.search(r"\b(static|inline|extern|typedef)\b", decl):
            return match.group(0)
        return f"{_ns.upper}DEF {decl.strip()}"

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
        if m and m.group(2).startswith((_ns.snake, _ns.upper)):
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


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def main() -> int:
    repo = _repo_root()
    parser = argparse.ArgumentParser(
        description=(
            f"Normalize {repo.name} headers by adding missing export macros on public "
            f"declarations and injecting missing unnamespaced aliases."
        )
    )
    parser.add_argument("paths", nargs="*", help="Header files to process (defaults to src/*.h)")
    parser.add_argument("--dry-run", action="store_true", help="Show what would change without writing")
    parser.add_argument("--namespace", default=None,
                        help=f"Library namespace (default: inferred from repo root: '{repo.name.lower()}')")
    args = parser.parse_args()

    ns = Namespace(args.namespace) if args.namespace else Namespace.infer(repo)
    _compile(ns)

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
            f"{path}: +{defs_count} {_ns.upper}DEF, {'~' if aliases_count else '='}{aliases_count} aliases, "
            f"{'+1 alias block' if block_created else '+0 alias block'}"
        )

        if not args.dry_run:
            path.write_text(updated, encoding="utf-8")

    if changed_files == 0:
        print("No normalization changes needed.")
    else:
        print(
            f"Updated {changed_files} file(s), inserted "
            f"{added_defs} {_ns.upper}DEF, {added_aliases} alias(es), "
            f"created {created_blocks} alias block(s)."
        )

    return 1 if args.dry_run and changed_files else 0


if __name__ == "__main__":
    raise SystemExit(main())
