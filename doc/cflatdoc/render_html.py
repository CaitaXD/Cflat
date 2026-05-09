"""Standalone HTML renderer — sidebar, dark mode, search, syntax highlight."""
from __future__ import annotations
import re
from pathlib import Path
from .model import ModuleDoc, EnumDoc, DocInfo
from .parsing import parse_doc_comment
from .sections import collect_module_sections, resolve_param_doc

_ASSETS = Path(__file__).parent / "assets"
CSS = (_ASSETS / "style.css").read_text(encoding="utf-8")
JS = (_ASSETS / "app.js").read_text(encoding="utf-8")

_current_module = ""
_type_map: dict[str, str] = {}
_fn_map: dict[str, str] = {}
_macro_map: dict[str, str] = {}


def _href(target_module: str, anchor: str) -> str:
    if target_module == _current_module:
        return f"#{anchor}"
    return f"{target_module}.html#{anchor}"


_BUILTIN_TYPES = {
    'u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64',
    'f32', 'f64', 'usize', 'isize', 'uptr', 'iptr',
    'byte', 'bool', 'char', 'void',
    'size_t', 'ssize_t', 'ptrdiff_t', 'intptr_t', 'uintptr_t',
    'atomic_bool', 'pthread_t', 'pthread_mutex_t', 'pthread_cond_t',
    'atomic_uint', 'atomic_ulong',
}


def _build_all_types(modules: list[ModuleDoc]) -> set[str]:
    types = set(_BUILTIN_TYPES)
    for mod in modules:
        for t in mod.typedefs:
            types.add(t.lower())
        for t in mod.struct_fields:
            types.add(t.lower())
        for e in mod.enums:
            types.add(e.name.lower())
        for n in mod.struct_tags.values():
            types.add(n.lower())
    return types


def _module_prefix_map(modules: list[ModuleDoc]) -> dict[str, str]:
    return {
        h.removesuffix(".h").removeprefix("Cflat"): m.module_name
        for m in modules if (h := m.header_name).startswith("Cflat")
    }


def _generic_prefixes(modules: list[ModuleDoc],
                      pmap: dict[str, str]) -> set[str]:
    rev = {v: k for k, v in pmap.items()}
    gen: set[str] = set()
    for mod in modules:
        p = rev.get(mod.module_name)
        if p is None:
            continue
        expected = f"CFLAT_{p.upper()}_FIELDS"
        if any(expected == s.split("(")[0].strip() for s in mod.macros):
            gen.add(p)
    return gen


def _is_generic_derived(name: str, pmap: dict[str, str],
                        gen: set[str], all_types: set[str]) -> bool:
    if not name.startswith("Cflat"):
        return False
    rest = name[5:]
    for p in sorted(gen, key=len, reverse=True):
        if rest.startswith(p):
            suffix = rest[len(p):]
            if suffix and suffix.lower() in all_types:
                return True
    return False


def _generic_slug(name: str, pmap: dict[str, str],
                  gen: set[str], all_types: set[str]) -> str | None:
    if not name.startswith("Cflat"):
        return None
    rest = name[5:]
    for p in sorted(gen, key=len, reverse=True):
        if rest.startswith(p):
            suffix = rest[len(p):]
            if suffix and suffix.lower() in all_types:
                return pmap[p]
    return None


def build_type_map(modules: list[ModuleDoc]) -> dict[str, str]:
    pmap = _module_prefix_map(modules)
    gen = _generic_prefixes(modules, pmap)
    all_types = _build_all_types(modules)
    m: dict[str, str] = {}
    for mod in modules:
        for t in mod.typedefs:
            target = _generic_slug(t, pmap, gen, all_types)
            m[t] = (target or mod.module_name, t)
        for t in mod.struct_fields:
            target = _generic_slug(t, pmap, gen, all_types)
            m[t] = (target or mod.module_name, t)
        for e in mod.enums:
            m[e.name] = (mod.module_name, e.name)
        for tag, name in mod.struct_tags.items():
            m[f"struct {tag}"] = (mod.module_name, name)
            target = _generic_slug(name, pmap, gen, all_types)
            if target:
                m[name] = (target, "")
            else:
                m[name] = (mod.module_name, name)
    return {k: _href(mod, anc) for k, (mod, anc) in m.items()}


def build_fn_map(modules: list[ModuleDoc]) -> dict[str, str]:
    m: dict[str, str] = {}
    for mod in modules:
        sec = collect_module_sections(mod)
        for fn, *_ in sec.parsed_decls:
            m[fn] = _href(mod.module_name, fn)
    return m


def build_macro_map(modules: list[ModuleDoc]) -> dict[str, str]:
    m: dict[str, str] = {}
    for mod in modules:
        for sig in mod.macros:
            name = _macro_name(sig)
            if name and not name.startswith("cflat__"):
                m[name] = _href(mod.module_name, name)
    return m


def _macro_name(sig: str) -> str:
    return sig.split("(", 1)[0].strip()


def esc(s: str) -> str:
    return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


C_KEYWORDS = {"const","void","static","inline","extern","return","if","else","for","while",
              "do","switch","case","break","continue","sizeof","typedef","struct","union",
              "enum","unsigned","signed","char","short","int","long","float","double","bool",
              "true","false","NULL","auto","register","volatile","restrict","_Bool"}


def highlight_c(code: str) -> str:
    """Tiny C signature tokenizer. Returns escaped HTML with spans."""
    out = []
    i, n = 0, len(code)
    struct_next = False
    while i < n:
        ch = code[i]
        if ch.isspace():
            out.append(ch); i += 1; continue
        if ch == '"' or ch == "'":
            j = i + 1
            while j < n and code[j] != ch:
                if code[j] == '\\' and j + 1 < n: j += 2
                else: j += 1
            out.append(f'<span class="tk-str">{esc(code[i:j+1])}</span>')
            i = j + 1; continue
        if ch.isdigit():
            struct_next = False
            j = i
        if ch.isalpha() or ch == '_':
            j = i
            while j < n and (code[j].isalnum() or code[j] == '_'): j += 1
            tok = code[i:j]
            if struct_next:
                struct_next = False
                key = f"struct {tok}"
                if key in _type_map:
                    out.append(f'<a class="tk-type" href="{_type_map[key]}">{esc(tok)}</a>')
                    i = j; continue
            cls = None
            if tok in C_KEYWORDS:
                cls = "tk-keyword"
                if tok == "struct":
                    struct_next = True
            elif tok in _type_map:
                out.append(f'<a class="tk-type" href="{_type_map[tok]}">{esc(tok)}</a>')
                i = j; continue
            elif tok in _fn_map:
                out.append(f'<a class="tk-fn" href="{_fn_map[tok]}">{esc(tok)}</a>')
                i = j; continue
            elif tok in _macro_map:
                out.append(f'<a class="tk-macro" href="{_macro_map[tok]}">{esc(tok)}</a>')
                i = j; continue
            elif tok.startswith("Cflat") or tok.endswith("_t") or tok in {"size_t","ssize_t","u8","u16","u32","u64","i8","i16","i32","i64","f32","f64","usize","isize","uptr","iptr"}:
                cls = "tk-type"
            elif tok.startswith("cflat_") or (j < n and code[j] == '('):
                cls = "tk-fn"
            out.append(f'<span class="{cls}">{esc(tok)}</span>' if cls else esc(tok))
            i = j; continue
        if ch in "(){}[],;*&=<>+-/%!?:.":
            struct_next = False
            out.append(f'<span class="tk-punct">{esc(ch)}</span>'); i += 1; continue
        out.append(esc(ch)); i += 1
    return "".join(out)


# -------- mini markdown -------------------------------------------------------

def _link_type_names(html: str) -> str:
    for mapping, cls in [
        (_type_map, "tk-type"),
        (_fn_map, "tk-fn"),
        (_macro_map, "tk-macro"),
    ]:
        if not mapping:
            continue
        for name, href in sorted(mapping.items(), key=lambda x: -len(x[0])):
            parts = re.split(r"(<[^>]*>)", html)
            for i, part in enumerate(parts):
                if i % 2 == 1:
                    continue
                parts[i] = re.sub(rf'\b{re.escape(name)}\b', f'<a class="{cls}" href="{href}">{name}</a>', part)
            html = "".join(parts)
    return html


def _inline_md(t: str) -> str:
    t = esc(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"\*(.+?)\*", r"<em>\1</em>", t)
    t = re.sub(r"`(.+?)`", r"<code>\1</code>", t)
    return _link_type_names(t)


def md_to_html(text: str) -> str:
    if not text:
        return ""
    parts = re.split(r"(```[a-zA-Z]*(?:\n|$))", text)
    out: list[str] = []
    in_code = False
    for part in parts:
        if part.startswith("```"):
            if not in_code:
                in_code = True
                out.append('<div class="sig"><button class="copy">copy</button><pre><code>')
            else:
                in_code = False
                out.append("</code></pre></div>")
            continue
        if in_code:
            out.append(highlight_c(part))
            continue
        for block in re.split(r"\n{2,}", part):
            block = block.strip()
            if not block:
                continue
            lines = block.splitlines()
            if all(re.match(r"^\s*[-*]\s+", l) for l in lines):
                stack = [0]
                out.append("<ul>")
                for l in lines:
                    indent = len(l) - len(l.lstrip())
                    item = re.sub(r"^\s*[-*]\s+", "", l)
                    if indent > stack[-1]:
                        out.append("<ul>")
                        stack.append(indent)
                    elif indent < stack[-1]:
                        while len(stack) > 1 and indent < stack[-1]:
                            out.append("</ul>")
                            stack.pop()
                    out.append(f"<li>{_inline_md(item)}</li>")
                while stack:
                    out.append("</ul>")
                    stack.pop()
                continue
            hm = re.match(r"^(#{1,6})\s+(.*)$", lines[0])
            if hm and len(lines) == 1:
                lvl = len(hm.group(1))
                out.append(f"<h{lvl}>{_inline_md(hm.group(2))}</h{lvl}>")
                continue
            out.append(f"<p>{_inline_md(' '.join(lines))}</p>")
    return "".join(out)


# -------- doc rendering -------------------------------------------------------

def _render_doc(info: DocInfo, typedef_docs, struct_fields) -> str:
    parts: list[str] = []
    if info.description:
        parts.append(_link_type_names(md_to_html(info.description)))
    if info.params:
        parts.append('<dl class="params">')
        for pn, pd in info.params.items():
            kind, inh = resolve_param_doc(pd, typedef_docs, struct_fields)
            if kind == "inherit":
                parts.append(f'<dt>{esc(pn)}</dt><dd>')
                parts.append('<dl class="params" style="margin:0">')
                for line in inh:
                    m = re.match(r"\s*\.(\w+):?\s*(.*)", line)
                    if m:
                        parts.append(f'<dt>.{esc(m.group(1))}</dt><dd>{_link_type_names(_inline_md(m.group(2)))}</dd>')
                parts.append('</dl></dd>')
            else:
                parts.append(f'<dt>{esc(pn)}</dt><dd>{_link_type_names(md_to_html(pd))}</dd>')
        parts.append('</dl>')
    if info.returns:
        parts.append('<dl class="params">')
        parts.append(f'<dt>Returns</dt><dd>{_link_type_names(md_to_html(info.returns))}</dd>')
        parts.append('</dl>')
    for label, items in [("Preconditions", info.preconditions),
                         ("Postconditions", info.postconditions),
                         ("Invariants", info.invariants)]:
        if items:
            parts.append(f'<div class="cond"><strong>{label}</strong><ul>')
            for it in items:
                parts.append(f'<li>{_link_type_names(_inline_md(it))}</li>')
            parts.append('</ul></div>')
    return "".join(parts)


def _entry(badge_class: str, badge_label: str, anchor: str, head_html: str, body_html: str) -> str:
    return (f'<div class="entry" id="{esc(anchor)}">'
            f'<div class="head"><span class="badge {badge_class}">{badge_label}</span>'
            f'{head_html}<a class="anchor" href="#{esc(anchor)}">#</a></div>'
            f'{body_html}</div>')


def _sig_block(code: str) -> str:
    return f'<div class="sig"><button class="copy">copy</button><pre><code>{highlight_c(code)}</code></pre></div>'


def _shell(title: str, sidebar: str, body: str, crumbs: str = "") -> str:
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{esc(title)}</title>
<style>{CSS}</style>
</head>
<body>
<div class="layout">
  <aside class="sidebar">{sidebar}</aside>
  <main class="main">
    <div class="topbar">
      <div class="crumbs">{crumbs}</div>
      <button id="theme-toggle" class="theme-btn">Toggle theme</button>
    </div>
    {body}
    <footer>Generated by <code>cflatdoc</code></footer>
  </main>
</div>
<script>{JS}</script>
</body>
</html>
"""


def _sidebar_for_module(doc: ModuleDoc, sec, type_names: list[str],
                        all_modules: list[ModuleDoc]) -> str:
    parts = [
        '<div class="brand"><span class="dot"></span> Cflat Docs</div>',
        '<input id="search" class="search" placeholder="Search (⌘K)" autocomplete="off">',
        '<div class="nav-section"><h4>Modules</h4>',
    ]
    for m in all_modules:
        cls = "active" if m.module_name == doc.module_name else ""
        parts.append(f'<a class="nav-link {cls}" href="{esc(m.module_name)}.html">{esc(m.module_name)}</a>')
    parts.append('</div>')

    def add_section(title, anchors):
        if not anchors:
            return
        parts.append(f'<div class="nav-section"><h4>{title}</h4>')
        for label, anchor in anchors:
            parts.append(f'<a class="nav-link" href="#{esc(anchor)}">{esc(label)}</a>')
        parts.append('</div>')

    add_section("Functions", [(f, f) for f, *_ in sec.parsed_decls])
    add_section("Types", [(t, t) for t in type_names] + [(e.name, e.name) for e in doc.enums])
    add_section("Opts", [(n, n) for n, *_ in sec.opt_entries])
    add_section("Scopes", [(n, n) for n, *_ in sec.scope_entries])
    add_section("Foreach", [(n, n) for n, *_ in sec.foreach_entries])
    add_section("Macros", [(m, m) for m in sec.renderable_macros])
    return "".join(parts)


def render_module_html(doc: ModuleDoc, all_modules: list[ModuleDoc]) -> str:
    global _type_map, _fn_map, _macro_map, _current_module
    _current_module = doc.module_name
    _type_map = build_type_map(all_modules)
    _fn_map = build_fn_map(all_modules)
    _macro_map = build_macro_map(all_modules)
    sec = collect_module_sections(doc)

    pmap = _module_prefix_map(all_modules)
    gen = _generic_prefixes(all_modules, pmap)
    all_types = _build_all_types(all_modules)
    type_names = [t for t in sec.all_type_names
                  if not _is_generic_derived(t, pmap, gen, all_types)]

    body: list[str] = [
        f'<h1>{esc(doc.title)}</h1>',
        f'<div class="meta">Header <code>{esc(doc.header_name)}</code> · Module <code>{esc(doc.module_name)}</code></div>',
    ]
    if doc.synopsis_comments:
        body.append('<div class="synopsis">' + _link_type_names(md_to_html("\n".join(doc.synopsis_comments))) + '</div>')

    toc = []
    if doc.declarations: toc.append('<a href="#functions">Functions</a>')
    if type_names or doc.enums: toc.append('<a href="#types">Types</a>')
    if sec.opt_entries: toc.append('<a href="#opts">Opts</a>')
    if sec.scope_entries: toc.append('<a href="#scopes">Scopes</a>')
    if sec.foreach_entries: toc.append('<a href="#foreach">Foreach</a>')
    if sec.renderable_macros: toc.append('<a href="#macros">Macros</a>')
    if toc:
        body.append('<div class="toc"><strong>Contents</strong><ul>'
                    + ''.join(f'<li>{t}</li>' for t in toc) + '</ul></div>')

    if doc.declarations:
        body.append('<h2 id="functions">Functions</h2>')
        for fn, ret, params, decl, _ in sec.parsed_decls:
            head = f'<h3>{esc(fn)}</h3>'
            inner = []
            if ret is not None and params is not None:
                inner.append(_sig_block(f"{ret} {fn}({params});"))
            d = doc.declaration_docs.get(decl, "")
            if d:
                inner.append(_render_doc(parse_doc_comment(d), doc.typedef_docs, doc.struct_fields))
            body.append(_entry("", "function", fn, head, "".join(inner)))

    if type_names or doc.enums:
        body.append('<h2 id="types">Types</h2>')
        for tn in type_names:
            head = f'<h3>{_link_type_names(esc(tn))}</h3>'
            inner: list[str] = []
            d = doc.typedef_docs.get(tn, "")
            info = parse_doc_comment(d) if d else DocInfo()
            if info.description:
                inner.append(_link_type_names(md_to_html(info.description)))
            fields = doc.struct_fields.get(tn, [])
            if fields:
                field_descs: dict[str, str] = {}
                for pn, pd in info.params.items():
                    kind, inh = resolve_param_doc(pd, doc.typedef_docs, doc.struct_fields)
                    if kind == "inherit":
                        for line in inh:
                            m = re.match(r"\s*\.(\w+):?\s*(.*)", line)
                            if m: field_descs[m.group(1)] = m.group(2)
                    else:
                        field_descs[pn] = pd
                rows = ['<table><thead><tr><th>Field</th><th>Type</th><th>Description</th></tr></thead><tbody>']
                for ft, fn in fields:
                    fd = field_descs.get(fn, "")
                    rows.append(f'<tr><td><code>{esc(fn)}</code></td><td><code>{_link_type_names(esc(ft))}</code></td><td>{_link_type_names(md_to_html(fd))}</td></tr>')
                rows.append('</tbody></table>')
                inner.append("".join(rows))
            macros = doc.backed_by_fields_macro.get(tn)
            if macros:
                links = []
                for m in macros:
                    href = _macro_map.get(m)
                    if href:
                        links.append(f'<a class="tk-macro" href="{href}">{esc(m)}</a>')
                    else:
                        links.append(esc(m))
                if len(links) == 1:
                    inner.append(f'<div class="generic-note">Backed by {links[0]}</div>')
                else:
                    inner.append(f'<div class="generic-note">Backed by '
                                 f'{", ".join(links[:-1])} + {links[-1]}</div>')
            body.append(_entry("type", "type", tn, head, "".join(inner)))
        for e in doc.enums:
            head = f'<h3>{_link_type_names(esc(e.name))} <span style="color:var(--muted);font-weight:400">: {_link_type_names(esc(e.base_type))}</span></h3>'
            inner = []
            if e.doc:
                inner.append(_link_type_names(md_to_html(e.doc)))
            if e.members:
                rows = ['<table><thead><tr><th>Member</th><th>Value</th></tr></thead><tbody>']
                for m in e.members:
                    n = m.split("=", 1)[0].strip()
                    v = m.split("=", 1)[1].strip() if "=" in m else ""
                    rows.append(f'<tr><td><code>{_link_type_names(esc(n))}</code></td><td><code>{_link_type_names(esc(v))}</code></td></tr>')
                rows.append('</tbody></table>')
                inner.append("".join(rows))
            body.append(_entry("enum", "enum", e.name, head, "".join(inner)))

    def render_entry_section(anchor, title, badge, entries):
        if not entries:
            return
        body.append(f'<h2 id="{anchor}">{title}</h2>')
        for name, ret, params, nested in entries:
            head = f'<h3>{esc(name)}</h3>'
            inner = []
            if ret is not None and params is not None:
                inner.append(_sig_block(f"{ret} {name}({params});"))
            if nested:
                code_lines = []
                for line in nested:
                    if line == "":
                        if code_lines:
                            inner.append(_sig_block("\n".join(code_lines)))
                            code_lines = []
                        continue
                    if line.startswith("  "):
                        code_lines.append(line)
                    else:
                        if code_lines:
                            inner.append(_sig_block("\n".join(code_lines)))
                            code_lines = []
                        inner.append(f'<div class="sig-label">{_link_type_names(esc(line))}</div>')
                if code_lines:
                    inner.append(_sig_block("\n".join(code_lines)))
            body.append(_entry(badge, badge, name, head, "".join(inner)))

    render_entry_section("opts", "Opts", "opt", sec.opt_entries)
    render_entry_section("scopes", "Scopes", "scope", sec.scope_entries)
    render_entry_section("foreach", "Foreach", "foreach", sec.foreach_entries)

    if sec.renderable_macros:
        body.append('<h2 id="macros">Macros</h2>')
        for macro in sec.renderable_macros:
            wrap = sec.resolved_macro_wrappers.get(macro)
            value = doc.macro_values.get(macro)
            sub = []
            if value: sub.append(f'<span style="color:var(--muted)"> = {_link_type_names(esc(value))}</span>')
            if wrap:
                tgt, defaults = wrap
                dt = ", ".join(f"{k}={v}" for k, v in defaults.items()) if defaults else "none"
                sub.append(f'<div style="color:var(--muted);font-size:.8em;margin-top:4px">defaults: {esc(dt)} → calls <code>{_link_type_names(esc(tgt))}</code></div>')
            head = f'<h3>{esc(macro)}{"".join(sub)}</h3>'
            d = doc.macro_docs.get(macro, "")
            inner = _link_type_names(md_to_html(d)) if d else ""
            body.append(_entry("macro", "macro", macro, head, inner))

    crumbs = f'<a href="index.html">Index</a> / <span>{esc(doc.module_name)}</span>'
    return _shell(doc.title, _sidebar_for_module(doc, sec, type_names, all_modules), "".join(body), crumbs)


def render_index_html(modules: list[ModuleDoc], synopsis: list[str]) -> str:
    global _type_map, _fn_map, _macro_map, _current_module
    _current_module = ""
    _type_map = build_type_map(modules)
    _fn_map = build_fn_map(modules)
    _macro_map = build_macro_map(modules)
    sidebar_parts = [
        '<div class="brand"><span class="dot"></span> Cflat Docs</div>',
        '<input id="search" class="search" placeholder="Search (⌘K)" autocomplete="off">',
        '<div class="nav-section"><h4>Modules</h4>',
    ]
    for m in modules:
        sidebar_parts.append(f'<a class="nav-link" href="{esc(m.module_name)}.html">{esc(m.module_name)}</a>')
    sidebar_parts.append('</div>')

    body = ['<h1>Cflat API Reference</h1>',
            '<div class="meta">Auto-generated module index</div>']
    if synopsis:
        body.append('<div class="synopsis">' + _link_type_names(md_to_html("\n".join(synopsis))) + '</div>')
    body.append('<h2>Modules</h2><div class="index-grid">')
    for m in modules:
        body.append(f'<a class="index-card" href="{esc(m.module_name)}.html">'
                    f'<code>{esc(m.module_name)}</code>'
                    f'<div class="desc">{esc(m.title)}</div></a>')
    body.append('</div>')

    return _shell("Cflat API Reference", "".join(sidebar_parts), "".join(body))
