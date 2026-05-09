"""Parse Cflat C headers into ModuleDoc objects.

This module preserves the regex-based parsing logic from the original
generate_man_pages.py — the patterns are well-tested. We just give them
homes and clearer names.
"""
from __future__ import annotations

import re
from pathlib import Path

from .model import DocInfo, EnumDoc, ModuleDoc

# ---------------------------------------------------------------- patterns ---

DOC_COMMENT = r"(?:/\*\*?(?:[^*]|\*(?!/))*\*/|(?:(?:///[^\n]*\n)+))"
DECL_PATTERN = r"(?:CFLAT_DEF\s+)?[^\n;{}]*\bcflat_[A-Za-z0-9_]+\b\s*\([^;{}]*\)\s*;"
MACRO_PATTERN = r"#\s*define\s+((?:cflat_|CFLAT_)[A-Za-z0-9_]+(?:\s*\([^)]*\))?)"
ENUM_PATTERN = r"cflat_enum\s*\(\s*(Cflat[A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)\s*\{(.*?)\};"
TYPEDEF_FULL = (
    r"typedef\s+(?:struct|union)\s+\w*\s*\{(?:[^{}]|\{[^{}]*\})*\}\s*(?:\w+\s+)*Cflat[A-Za-z0-9_]+\s*;"
    r"|"
    r"typedef\b(?!\s+(?:struct|union)\s+\w*\s*\{)[^;]*\bCflat[A-Za-z0-9_]+\s*;"
)

DECL_RE = re.compile(rf"(?m)^(?![ \t])(?!\s*#)(?!\s*typedef\b)\s*{DECL_PATTERN}")
TYPEDEF_RE = re.compile(r"(?m)^\s*typedef\b[^;{]*\b(Cflat[A-Za-z0-9_]+)\s*;")
TYPEDEF_STRUCT_RE = re.compile(
    r"(?ms)^\s*typedef\s+(?:struct|union)\s+\w*\s*\{(?:[^{}]|\{[^{}]*\})*\}\s*(?:\w+\s+)*(Cflat[A-Za-z0-9_]+)\s*;"
)
TYPEDEF_DOC_RE = re.compile(rf"(?ms)(?P<doc>{DOC_COMMENT})\s*(?P<typedef>{TYPEDEF_FULL})")
MACRO_DEF_RE = re.compile(rf"(?m)^\s*{MACRO_PATTERN}")
SIMPLE_MACRO_RE = re.compile(
    r"(?m)^\s*#\s*define\s+((?:cflat_|CFLAT_)[A-Za-z0-9_]+)\s+([^\n\\]+?)\s*$"
)
MACRO_BODY_RE = re.compile(
    r"(?m)^\s*#\s*define\s+((?:cflat_|CFLAT_)[A-Za-z0-9_]+(?:\s*\([^)]*\))?)\s+([^\n]+)$"
)
ENUM_RE = re.compile(rf"(?ms){ENUM_PATTERN}")
DECL_DOC_RE = re.compile(rf"(?ms)(?P<doc>{DOC_COMMENT})\s*(?P<decl>{DECL_PATTERN})")
MACRO_DOC_RE = re.compile(rf"(?ms)(?P<doc>{DOC_COMMENT})\s*(?P<macro>{MACRO_PATTERN})")
ENUM_DOC_RE = re.compile(rf"(?ms)(?P<doc>{DOC_COMMENT})\s*(?P<enum>{ENUM_PATTERN})")
TYPEDEF_FWD_RE = re.compile(
    r"typedef\s+(?:struct|union)\s+(\w+)\s+(Cflat[A-Za-z0-9_]+)\s*;"
)
STRUCT_DEF_RE = re.compile(
    r"(?ms)^\s*struct\s+(\w+)\s*\{(.*?)\}\s*;"
)
PUBLIC_SPLIT_RE = re.compile(r"(?m)^#\s*(?:if\s+defined\(|ifdef\s+)CFLAT_IMPLEMENTATION\b")

# ---------- struct body macro expanders --------------------------------------

STRUCT_MACRO_EXPANDERS: list[tuple[re.Pattern, str | callable]] = [
    (re.compile(r"CFLAT_SLICE_HEADER_FIELDS"),          "usize capacity;\n    usize length"),
    (re.compile(r"CFLAT_SLICE_FIELDS\(([^)]+)\)"),      "usize capacity;\n    usize length;\n    \\1 *data"),
]

def expand_struct_body(body: str) -> str:
    for pat, repl in STRUCT_MACRO_EXPANDERS:
        body = pat.sub(repl, body)
    return body


def _collect_guard_regions(text: str) -> list[tuple[int, int]]:
    """Return (start, end) byte ranges of ``#ifndef X / #define X … #endif`` blocks."""
    regions: list[tuple[int, int]] = []
    lines = text.splitlines(keepends=True)
    i = 0
    while i < len(lines):
        if re.match(r"^\s*#ifndef\s+", lines[i]):
            if i + 1 < len(lines) and re.match(r"^\s*#define\s+", lines[i + 1]):
                start = sum(len(l) for l in lines[:i])
                depth = 0
                for j in range(i + 2, len(lines)):
                    if re.match(r"^\s*#if\b", lines[j]):
                        depth += 1
                    elif re.match(r"^\s*#endif\b", lines[j]):
                        if depth == 0:
                            end = sum(len(l) for l in lines[: j + 1])
                            regions.append((start, end))
                            i = j
                            break
                        depth -= 1
        i += 1
    return regions

# ---------------------------------------------------------------- helpers ----

def normalize_decl(raw: str) -> str:
    return " ".join(raw.strip().split())


def dedent_preserve_relative(lines: list[str]) -> list[str]:
    indents = [len(re.match(r"^[ \t]*", l).group(0)) for l in lines if l.strip()]
    if not indents:
        return lines
    common = min(indents)
    return [l[common:] if len(l) >= common else "" for l in lines]


def strip_block_comment_prefix(line: str) -> str:
    line = line.rstrip()
    if re.match(r"^\s*\*", line):
        line = re.sub(r"^\s*\*", "", line, count=1)
        if line.startswith(" "):
            line = line[1:]
    return line


def strip_line_comment_prefix(line: str) -> str:
    if line.startswith("///"):
        line = line[3:]
    elif line.startswith("//"):
        line = line[2:]
    if line.startswith(" "):
        line = line[1:]
    return line.rstrip()


def clean_doc_comment(raw: str) -> str:
    text = raw.strip()
    if text.startswith("/*"):
        prefix = 3 if text.startswith("/**") else 2
        text = text[prefix:-2] if text.endswith("*/") else text[prefix:]
        lines = [strip_block_comment_prefix(l) for l in text.splitlines()]
    else:
        lines = [strip_line_comment_prefix(l) for l in text.splitlines()]
    lines = dedent_preserve_relative(lines)
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if len(lines) >= 2 and lines[0].strip() == "```" and lines[-1].strip() == "```":
        lines = lines[1:-1]
    return "\n".join(lines).rstrip()


def parse_doc_comment(doc_text: str) -> DocInfo:
    if not doc_text:
        return DocInfo()
    desc: list[str] = []
    info = DocInfo()
    tag = None
    tag_key: str | None = None
    tag_body: list[str] = []

    def flush():
        nonlocal tag, tag_key, tag_body
        body = " ".join(tag_body).strip()
        if tag == "param" and tag_key:
            info.params[tag_key] = body
        elif tag == "return":
            info.returns = body
        elif tag == "precondition":
            info.preconditions.append(body)
        elif tag == "postcondition":
            info.postconditions.append(body)
        elif tag == "invariant":
            info.invariants.append(body)
        tag = None
        tag_key = None
        tag_body = []

    tags = [
        ("param", r"^@param\s+(\w+)\s*:\s*(.*)"),
        ("return", r"^@return\s*:?\s*(.*)"),
        ("precondition", r"^@precondition\s*(.*)"),
        ("postcondition", r"^@postcondition\s*(.*)"),
        ("invariant", r"^@invariant\s*(.*)"),
    ]
    for line in doc_text.splitlines():
        matched = False
        for name, pat in tags:
            m = re.match(pat, line)
            if not m:
                continue
            flush()
            tag = name
            if name == "param":
                tag_key = m.group(1)
                tag_body = [m.group(2)]
            else:
                tag_body = [m.group(1)]
            matched = True
            break
        if matched:
            continue
        if re.match(r"^@\w+", line):
            flush()
            continue
        if tag:
            tag_body.append(line.strip())
        elif line.strip():
            desc.append(line)
    flush()
    info.description = "\n".join(desc).strip()
    return info


def parse_enum_members(body: str) -> list[str]:
    cleaned: list[str] = []
    for line in body.splitlines():
        line = re.sub(r"//.*$", "", line).strip()
        if line:
            cleaned.append(line)
    body = " ".join(cleaned)

    members: list[str] = []
    token: list[str] = []
    depth = 0
    for ch in body:
        if ch == "(":
            depth += 1
        elif ch == ")" and depth > 0:
            depth -= 1
        if ch == "," and depth == 0:
            m = " ".join("".join(token).strip().split())
            if m and "cflat__" not in m and "CFLAT__" not in m:
                members.append(m)
            token = []
            continue
        token.append(ch)
    tail = " ".join("".join(token).strip().split())
    if tail and "cflat__" not in tail and "CFLAT__" not in tail:
        members.append(tail)
    return members


def is_simple_macro_value(value: str) -> bool:
    if "\\" in value or "##" in value or value.startswith("#"):
        return False
    return bool(re.fullmatch(r"[A-Za-z0-9_\"'().,+\-*/%<>&|^~!?:\s]+", value))


def macro_name_from_signature(sig: str) -> str:
    return sig.split("(", 1)[0].strip()


def parse_macro_signature(sig: str) -> tuple[str, str | None]:
    m = re.match(r"^(?P<name>[A-Za-z0-9_]+)(?:\((?P<params>.*)\))?$", sig)
    if not m:
        return sig, None
    return m.group("name"), m.group("params")


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    cur: list[str] = []
    p = b = k = 0
    for ch in text:
        if ch == "," and p == b == k == 0:
            s = "".join(cur).strip()
            if s:
                parts.append(s)
            cur = []
            continue
        if ch == "(":
            p += 1
        elif ch == ")" and p > 0:
            p -= 1
        elif ch == "{":
            b += 1
        elif ch == "}" and b > 0:
            b -= 1
        elif ch == "[":
            k += 1
        elif ch == "]" and k > 0:
            k -= 1
        cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        parts.append(tail)
    return parts


def strip_outer_parens(text: str) -> str:
    v = text.strip()
    while v.startswith("(") and v.endswith(")"):
        depth = 0
        ok = True
        for i, ch in enumerate(v):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth < 0:
                    ok = False
                    break
            if depth == 0 and i != len(v) - 1:
                ok = False
                break
        if not ok:
            break
        v = v[1:-1].strip()
    return v


def parse_param_decl(param: str) -> tuple[str, str] | None:
    p = param.strip()
    if not p or p == "void":
        return None
    m = re.match(r"^(?P<type>.+?)(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?$", p)
    if not m:
        return None
    return " ".join(m.group("type").split()), m.group("name")


def parse_param_decls(params: str) -> list[tuple[str, str]]:
    out = []
    for part in split_top_level_commas(params):
        e = parse_param_decl(part)
        if e:
            out.append(e)
    return out


def format_typed_param(field_type: str, field_name: str, default: str | None = None) -> str:
    t = " ".join(field_type.split())
    rendered = f"{t}{field_name}" if t.endswith("*") else f"{t} {field_name}"
    return f"{rendered}={default}" if default is not None else rendered


def extract_opt_type(params: str) -> str | None:
    m = re.search(r"(?:^|,)\s*([A-Za-z_][A-Za-z0-9_\s\*]+?)\s+opt\s*$", params.strip())
    return " ".join(m.group(1).split()) if m else None


def zero_value_for_type(t: str) -> str:
    n = " ".join(t.split())
    if "*" in n:
        return "NULL"
    low = n.lower()
    if re.search(r"\bbool\b", low):
        return "false"
    if re.search(r"\b(float|double|f16|f32|f64)\b", low):
        return "0.0"
    if re.search(r"\b(u|i)\d+\b", low) or re.search(
        r"\b(char|short|int|long|size_t|ssize_t|usize|isize|uptr|iptr)\b", low
    ):
        return "0"
    return "{0}"


def parse_macro_param_names(params: str | None) -> list[str]:
    if not params:
        return []
    return [p.strip() for p in split_top_level_commas(params) if p.strip()]


def find_call_args(body: str, function_name: str) -> list[str] | None:
    pat = re.compile(rf"\b{re.escape(function_name)}\s*\(")
    for m in pat.finditer(body):
        i = m.end()
        depth = 1
        chars: list[str] = []
        while i < len(body):
            ch = body[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return split_top_level_commas("".join(chars))
            chars.append(ch)
            i += 1
    return None


def is_generic_macro_param(name: str, body: str) -> bool:
    if not re.match(r"^T[A-Za-z0-9_]*$", name):
        return False
    pats = (
        rf"\bsizeof\s*\(\s*{re.escape(name)}\s*\)",
        rf"\bcflat_alignof\s*\(\s*{re.escape(name)}\s*\)",
        rf"\(\s*{re.escape(name)}\s*\*\s*\)\s*NULL",
    )
    return any(re.search(p, body) for p in pats)


def extract_opt_defaults(body: str) -> dict[str, str]:
    defaults: dict[str, str] = {}
    for m in re.finditer(r"\.([A-Za-z_][A-Za-z0-9_]*)\s*=", body):
        field = m.group(1)
        i = m.end()
        p = b = k = 0
        chars: list[str] = []
        while i < len(body):
            ch = body[i]
            if ch == "," and p == b == k == 0:
                break
            if ch == "(":
                p += 1
            elif ch == ")" and p > 0:
                p -= 1
            elif ch == "{":
                b += 1
            elif ch == "}" and b > 0:
                b -= 1
            elif ch == "[":
                k += 1
            elif ch == "]" and k > 0:
                k -= 1
            chars.append(ch)
            i += 1
        v = "".join(chars).strip()
        for closer, opener in [(")", "("), ("]", "["), ("}", "{")]:
            while v.endswith(closer) and v.count(opener) < v.count(closer):
                v = v[:-1].rstrip()
        if v:
            defaults[field] = v
    return defaults


def parse_macro_opt_wrappers(public_text: str, macros: list[str]) -> dict:
    wrappers: dict = {}
    macro_set = set(macros)
    for sig, body in MACRO_BODY_RE.findall(public_text):
        sig = " ".join(sig.strip().split())
        if sig not in macro_set:
            continue
        name = macro_name_from_signature(sig)
        target = f"{name}_opt"
        if re.search(rf"\b{re.escape(target)}\s*\(", body):
            wrappers[sig] = (target, extract_opt_defaults(body))
    return wrappers


def extract_struct_fields(body: str) -> list[tuple[str, str]]:
    body = re.sub(r"(?ms)/\*.*?\*/", "", body)
    body = re.sub(r"//[^\n]*", "", body)
    fields: list[tuple[str, str]] = []

    def _add_field(ftype: str, decl: str) -> None:
        star = ""
        while decl.startswith("*"):
            star += "*"
            decl = decl[1:]
        if star:
            ftype = f"{ftype} {star}".strip()
        fields.append((" ".join(ftype.split()), decl))

    for line in body.splitlines():
        line = line.strip()
        if not line or not line.endswith(";"):
            continue
        line = line[:-1].strip()
        line = re.sub(r"\s*\[[^\]]+\]", "", line).strip()
        line = re.sub(r"\s*:\s*\d+", "", line).strip()
        if not line:
            continue
        if "," in line:
            groups = [g.strip() for g in line.split(",")]
            first_parts = groups[0].split()
            if len(first_parts) < 2:
                continue
            base_type = " ".join(first_parts[:-1])
            for i, g in enumerate(groups):
                gp = g.split()
                decl = gp[-1]
                ftype = " ".join(gp[:-1]) if i == 0 else base_type
                _add_field(ftype, decl)
        else:
            parts = line.split()
            if len(parts) < 2:
                continue
            decl = parts[-1]
            ftype = " ".join(parts[:-1])
            _add_field(ftype, decl)
    return fields


def detect_fields_macro_backed(
    public_text: str, struct_tags: dict[str, str]
) -> dict[str, list[str]]:
    """Detect types whose struct body consists entirely of CFLAT_*_FIELDS calls.
    
    Returns {typedef_name: [macro_name, ...]} — the macro names listed in order,
    deduped. Handles both combined ``typedef struct {…} Name;`` and standalone
    ``struct tag {…};`` + forward-decl patterns.
    """
    from collections import OrderedDict

    result: dict[str, list[str]] = {}

    def _check(name: str, raw_body: str) -> None:
        clean = re.sub(r"(?ms)/\*.*?\*/", "", raw_body)
        clean = re.sub(r"//[^\n]*", "", clean).strip()
        if not clean:
            return
        remainder = re.sub(
            r"\s*CFLAT_\w+_FIELDS(?:\([^)]*\))?\s*;?\s*", "", clean
        ).strip()
        if remainder:
            return
        macros = re.findall(r"CFLAT_\w+_FIELDS", clean)
        if macros:
            result[name] = list(OrderedDict.fromkeys(macros))

    sre = re.compile(
        r"(?ms)typedef\s+(?:struct|union)\b[^{]*\{(?P<body>.*?)\}"
        r"\s*(?:\w+\s+)*(?P<name>Cflat[A-Za-z0-9_]+)\s*;"
    )
    for m in sre.finditer(public_text):
        _check(m.group("name"), m.group("body"))

    for m in STRUCT_DEF_RE.finditer(public_text):
        tag = m.group(1)
        if tag in struct_tags:
            _check(struct_tags[tag], m.group(2))

    return result


def parse_struct_fields(public_text: str) -> dict[str, list[tuple[str, str]]]:
    out: dict = {}
    sre = re.compile(r"(?ms)typedef\s+(?:struct|union)\b[^{]*\{(?P<body>.*?)\}\s*(?:\w+\s+)*(?P<name>Cflat[A-Za-z0-9_]+)\s*;")
    for m in sre.finditer(public_text):
        body = expand_struct_body(m.group("body"))
        f = extract_struct_fields(body)
        if f:
            out[m.group("name")] = f
    return out


def parse_opt_struct_fields(public_text: str) -> dict[str, list[tuple[str, str]]]:
    out: dict = {}
    sre = re.compile(r"(?ms)typedef\s+struct\b[^{]*\{(?P<body>.*?)\}\s*(?P<name>Cflat[A-Za-z0-9_]+)\s*;")
    for m in sre.finditer(public_text):
        name = m.group("name")
        if not name.endswith("Opt"):
            continue
        body = re.sub(r"//.*$", "", m.group("body"), flags=re.M)
        fields = [
            (" ".join(ft.split()), fn)
            for ft, fn in re.findall(
                r"(?m)^\s*([A-Za-z_][A-Za-z0-9_\s\*]*?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;",
                body,
            )
        ]
        if fields:
            out[name] = fields
    return out


def parse_macro_bodies(public_text: str) -> dict[str, str]:
    bodies: dict = {}
    lines = public_text.splitlines()
    define_re = re.compile(
        r"^\s*#\s*define\s+((?:cflat_|CFLAT_)[A-Za-z0-9_]+(?:\s*\([^)]*\))?)\s*(.*)$"
    )
    i = 0
    while i < len(lines):
        m = define_re.match(lines[i])
        if not m:
            i += 1
            continue
        sig = " ".join(m.group(1).strip().split())
        body = m.group(2).rstrip()
        while body.endswith("\\") and i + 1 < len(lines):
            body = body[:-1].rstrip()
            i += 1
            body = f"{body} {lines[i].strip()}".strip()
        bodies[sig] = " ".join(body.strip().split())
        i += 1
    return bodies


def extract_top_level_comments(text: str) -> list[str]:
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        while i < n and text[i] in " \t\r\n":
            i += 1
        if i >= n:
            break
        if text.startswith("//", i):
            while i < n and text.startswith("//", i):
                end = text.find("\n", i)
                if end == -1:
                    end = n
                out.append(strip_line_comment_prefix(text[i:end]))
                i = end + 1
            out.append("")
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end == -1:
                end = n - 2
            block = [strip_block_comment_prefix(l) for l in text[i + 2:end].splitlines()]
            body = dedent_preserve_relative(block)
            while body and not body[0].strip():
                body.pop(0)
            while body and not body[-1].strip():
                body.pop()
            if len(body) >= 2 and body[0].strip() == "```" and body[-1].strip() == "```":
                body = body[1:-1]
            out.extend(body)
            out.append("")
            i = end + 2
            continue
        break
    while out and not out[-1].strip():
        out.pop()
    return out


def parse_function_decl(decl: str) -> tuple[str, str, str] | None:
    m = re.match(
        r"^(?P<prefix>.*?)\b(?P<name>cflat_[A-Za-z0-9_]+)\s*\((?P<params>.*)\)\s*;$",
        decl,
    )
    if not m:
        return None
    return_type = re.sub(r"^CFLAT_DEF\s+", "", m.group("prefix")).strip()
    return m.group("name"), return_type, m.group("params").strip()


# ---------------------------------------------------------------- header -----

def parse_header(header: Path) -> ModuleDoc:
    text = header.read_text(encoding="utf-8")
    public = PUBLIC_SPLIT_RE.split(text, maxsplit=1)[0]

    synopsis = extract_top_level_comments(text)

    declaration_docs: dict[str, str] = {}
    for m in DECL_DOC_RE.finditer(public):
        decl = normalize_decl(m.group("decl"))
        if "cflat__" in decl:
            continue
        d = clean_doc_comment(m.group("doc"))
        if d:
            declaration_docs[decl] = d

    macro_docs: dict[str, str] = {}
    for m in MACRO_DOC_RE.finditer(public):
        macro = " ".join(m.group("macro").strip().split())
        if macro.startswith("cflat__") or macro.startswith("CFLAT__"):
            continue
        d = clean_doc_comment(m.group("doc"))
        if d:
            macro_docs[macro] = d

    decls = sorted({normalize_decl(d) for d in DECL_RE.findall(public) if "cflat__" not in d})
    typedefs = sorted(set(TYPEDEF_RE.findall(public)))
    guarded = _collect_guard_regions(public)
    for m in TYPEDEF_STRUCT_RE.finditer(public):
        name = m.group(1)
        if name not in typedefs:
            in_guard = any(s <= m.start() < e for s, e in guarded)
            if not in_guard:
                typedefs.append(name)
    macros = sorted({
        " ".join(m.strip().split())
        for m in MACRO_DEF_RE.findall(public)
        if not m.startswith("cflat__")
        and not m.startswith("CFLAT__")
        and not m.endswith("_H")
        and not m.endswith("_IMPLEMENTATION")
        and not m.endswith("_NO_ALIAS")
    })
    macro_values = {
        n: " ".join(v.strip().split())
        for n, v in SIMPLE_MACRO_RE.findall(public)
        if n in macros and is_simple_macro_value(" ".join(v.strip().split()))
    }
    macro_bodies = parse_macro_bodies(public)
    macro_opt_wrappers = parse_macro_opt_wrappers(public, macros)
    opt_fields = parse_opt_struct_fields(public)
    struct_fields = parse_struct_fields(public)

    typedef_docs: dict[str, str] = {}
    for m in TYPEDEF_DOC_RE.finditer(public):
        td = m.group("typedef")
        nm = TYPEDEF_RE.search(td) or TYPEDEF_STRUCT_RE.search(td)
        if nm:
            d = clean_doc_comment(m.group("doc"))
            if d:
                typedef_docs[nm.group(1)] = d

    enums: list[EnumDoc] = []
    enum_doc_by_name: dict[str, str] = {}
    for m in ENUM_DOC_RE.finditer(public):
        d = clean_doc_comment(m.group("doc"))
        if d:
            enum_doc_by_name[m.group(2)] = d
    for m in ENUM_RE.finditer(public):
        name = m.group(1)
        base = m.group(2)
        members = parse_enum_members(m.group(3))
        enums.append(EnumDoc(
            name=name, base_type=base,
            signature=f"cflat_enum({name}, {base})",
            members=members, doc=enum_doc_by_name.get(name, ""),
        ))

    struct_tags: dict[str, str] = {}
    for m in TYPEDEF_FWD_RE.finditer(public):
        tag, name = m.group(1), m.group(2)
        if not tag.startswith("cflat__"):
            struct_tags[tag] = name
    for m in TYPEDEF_STRUCT_RE.finditer(public):
        decl = m.group(0)
        tag_m = re.search(r"typedef\s+(?:struct|union)\s+(\w+)", decl)
        if tag_m and not tag_m.group(1).startswith("cflat__"):
            struct_tags[tag_m.group(1)] = m.group(1)
    for m in STRUCT_DEF_RE.finditer(public):
        tag, body = m.group(1), m.group(2)
        if tag in struct_tags:
            typedef_name = struct_tags[tag]
            f = extract_struct_fields(expand_struct_body(body))
            if f:
                cur = struct_fields.get(typedef_name, [])
                struct_fields[typedef_name] = cur + f

    backed_by_fields_macro = detect_fields_macro_backed(public, struct_tags)

    # Drop guarded types from struct_fields so they don't get doc entries
    for m in TYPEDEF_STRUCT_RE.finditer(public):
        name = m.group(1)
        in_guard = any(s <= m.start() < e for s, e in guarded)
        if in_guard and name in struct_fields:
            del struct_fields[name]

    stem = header.stem
    module = stem.removeprefix("Cflat")
    slug = f"cflat-{re.sub(r'([a-z0-9])([A-Z])', r'\1-\2', module).lower()}"
    return ModuleDoc(
        module_name=slug,
        header_name=header.name,
        title=f"Cflat {module} module",
        synopsis_comments=synopsis,
        typedefs=typedefs,
        declarations=decls,
        declaration_docs=declaration_docs,
        macros=macros,
        macro_bodies=macro_bodies,
        macro_values=macro_values,
        macro_opt_wrappers=macro_opt_wrappers,
        opt_struct_fields=opt_fields,
        struct_fields=struct_fields,
        macro_docs=macro_docs,
        typedef_docs=typedef_docs,
        enums=enums,
        struct_tags=struct_tags,
        backed_by_fields_macro=backed_by_fields_macro,
    )
