"""Compute renderable sections (functions, opts, scopes, foreach, macros)
from a parsed ModuleDoc. Faithful port of the original `_collect_module_sections`.
"""
from __future__ import annotations
import re

from .model import ModuleDoc, ModuleSections, Namespace
from .parsing import (
    parse_function_decl, parse_macro_signature, parse_macro_param_names,
    parse_param_decls, parse_param_decl, split_top_level_commas,
    macro_name_from_signature, extract_opt_defaults, find_call_args,
    is_generic_macro_param, format_typed_param, extract_opt_type,
    zero_value_for_type, strip_outer_parens,
)


_ns: Namespace = Namespace("cflat")


def collect_module_sections(doc: ModuleDoc, ns: Namespace | None = None) -> ModuleSections:
    global _ns
    if ns:
        _ns = ns
    resolved_macro_wrappers = dict(doc.macro_opt_wrappers)
    opt_entries: list = []
    scope_entries: list = []
    foreach_entries: list = []
    parsed_decls: list = []
    parsed_function_by_name: dict = {}

    if doc.declarations:
        for decl in doc.declarations:
            if f"{_ns.snake}_" in decl:
                continue
            parsed = parse_function_decl(decl)
            if parsed:
                fn, ret, params = parsed
                parsed_decls.append((fn, ret, params, decl, None))
                parsed_function_by_name[fn] = (ret, params)
            else:
                parsed_decls.append((decl, None, None, decl, None))

        scope_macro_entries: list = []
        for sig in doc.macros:
            name, params = parse_macro_signature(sig)
            if not name.endswith("_scope"):
                continue
            scope_sig = name if params is None else f"{name}({params})"
            scope_macro_entries.append((scope_sig, name, params, doc.macro_bodies.get(sig, "")))

        foreach_macro_entries: list = []
        for sig in doc.macros:
            name, params = parse_macro_signature(sig)
            if not name.endswith("_foreach"):
                continue
            fe_sig = name if params is None else f"{name}({params})"
            foreach_macro_entries.append((fe_sig, name, params, doc.macro_bodies.get(sig, "")))

        macro_name_to_wrapper = {
            macro_name_from_signature(s): w for s, w in resolved_macro_wrappers.items()
        }
        wrapper_bridge_calls: dict[str, str] = {}

        for sig in doc.macros:
            _, params = parse_macro_signature(sig)
            if not params or "..." not in params:
                continue
            if sig in resolved_macro_wrappers:
                continue
            body = doc.macro_bodies.get(sig, "")
            if not body or f"{_ns.snake}defer(" in body:
                continue
            local_defaults = extract_opt_defaults(body)
            for call_name in re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body):
                inh = macro_name_to_wrapper.get(call_name)
                if inh is None and not call_name.startswith(_ns.snake):
                    inh = macro_name_to_wrapper.get(f"{_ns.snake}{call_name}")
                if inh is None and call_name.endswith("_opt"):
                    inh = (call_name, {})
                if inh is None:
                    continue
                target, defaults = inh
                merged = (target, {**defaults, **local_defaults})
                resolved_macro_wrappers[sig] = merged
                macro_name_to_wrapper[macro_name_from_signature(sig)] = merged
                wrapper_bridge_calls[sig] = call_name
                break

        promoted_interfaces: dict = {}
        for sig, (target, defaults) in resolved_macro_wrappers.items():
            target_fn = parsed_function_by_name.get(target)
            if not target_fn:
                continue
            name, params = parse_macro_signature(sig)
            if not params or "..." not in params:
                continue
            ret_type, opt_params = target_fn
            opt_type = extract_opt_type(opt_params) or "opt"
            full_opt_fields = doc.opt_struct_fields.get(opt_type, [])
            opt_param_entries = parse_param_decls(opt_params)
            base_params = [(t, n) for t, n in opt_param_entries if n != "opt"]

            macro_param_names = [n for n in parse_macro_param_names(params) if n != "..."]
            body = doc.macro_bodies.get(sig, "")
            bridge = wrapper_bridge_calls.get(sig, target.removesuffix("_opt"))
            bridge_pref = bridge if bridge.startswith(_ns.snake) else f"{_ns.snake}{bridge}"
            bridge_args = find_call_args(body, bridge) or find_call_args(body, bridge_pref) or []

            rendered: list[str] = []
            subs: dict[str, str] = {}
            if name == target.removesuffix("_opt"):
                rendered.extend(format_typed_param(t, n) for t, n in base_params)
                for idx, mp in enumerate(macro_param_names):
                    subs[mp] = base_params[idx][1] if idx < len(base_params) else mp
            else:
                for mp in macro_param_names:
                    if is_generic_macro_param(mp, body):
                        rendered.append(f"Type({mp})")
                        subs[mp] = mp
                        continue
                    mapped: tuple[str, str] | None = None
                    for idx, ae in enumerate(bridge_args):
                        if idx >= len(base_params):
                            break
                        pt, pn = base_params[idx]
                        ex = strip_outer_parens(ae)
                        if ex == mp:
                            mapped = (pt, pn)
                            break
                        if mapped is None and re.search(rf"\b{re.escape(mp)}\b", ae):
                            mapped = (pt, mp.lower())
                    if mapped is None:
                        rendered.append(f"Token({mp})")
                        subs[mp] = mp
                        continue
                    mt, mn = mapped
                    rendered.append(format_typed_param(mt, mn))
                    subs[mp] = mn

            for ft, fn in full_opt_fields:
                dv = defaults.get(fn, zero_value_for_type(ft))
                rendered.append(format_typed_param(ft, fn, dv))

            interface_params = ", ".join(rendered)
            assignments = ", ".join(f".{fn}={fn}" for _, fn in full_opt_fields)
            positional: list[str] = []
            for p in macro_param_names:
                positional.append(p if is_generic_macro_param(p, body) else subs.get(p, p.lower()))
            call_without = f"{name}({', '.join(positional)})"
            nested_without = call_without if full_opt_fields else None
            if assignments:
                positional.append(assignments)
            nested_call = f"{name}({', '.join(positional)})"
            promoted_interfaces[name] = (interface_params, ret_type, target, opt_params,
                                          full_opt_fields, nested_call, nested_without)

        for iname, (ipa, rt, tgt, op, _of, ncall, nwo) in sorted(promoted_interfaces.items()):
            nested = ["Caller:", f"  {ncall}"]
            if nwo:
                nested.append(f"  {nwo}")
            nested.extend(["", "Function:", f"  {rt} {tgt} ({op})"])
            opt_entries.append((iname, rt, ipa, nested))

        promoted_by_target = {
            tgt: (iname, rt, ipa)
            for iname, (ipa, rt, tgt, *_rest) in promoted_interfaces.items()
        }

        for scope_sig, name, params, body in sorted(scope_macro_entries, key=lambda e: e[0]):
            display = scope_sig
            nested: list[str] = []
            if params is not None:
                raw = split_top_level_commas(params)
                parts = [p for p in raw if p != "..."]
                varargs = len(parts) != len(raw)
                promo: list[str] = []
                if varargs:
                    for cn in re.findall(rf"\b({_ns.snake}[A-Za-z0-9_]+)\s*\(", body):
                        p = promoted_by_target.get(cn)
                        if not p:
                            continue
                        _, _, pp = p
                        promo = split_top_level_commas(pp)
                        break
                if promo:
                    display = f"{name}({', '.join([*parts, *promo])})"
                    promo_names = [
                        pn for entry in promo
                        for parsed in [parse_param_decl(entry.split('=', 1)[0].strip())]
                        if parsed for _, pn in [parsed]
                    ]
                    explicit = [*parts, *[f".{n}={n}" for n in promo_names]]
                    calls = [
                        f"{name}({', '.join(explicit)})",
                        f"{name}({', '.join(parts)})",
                    ]
                    nested = ["Caller:"]
                    for c in calls:
                        nested.extend([f"  {c} {{", "    ...", "  }"])
                else:
                    calls = [f"{name}({params})"]
                    if varargs:
                        calls.append(f"{name}({', '.join(parts)})")
                    nested = ["Caller:"]
                    for c in calls:
                        nested.extend([f"  {c} {{", "    ...", "  }"])
            else:
                nested = ["Caller:", f"  {name} {{", "    ...", "  }"]

            ordered: list[str] = []
            for cn in re.findall(rf"\b({_ns.snake}[A-Za-z0-9_]+)\s*\(", body):
                if cn in parsed_function_by_name and cn not in ordered:
                    ordered.append(cn)
            if ordered:
                if nested:
                    nested.append("")
                nested.append("Function:")
                for cn in ordered:
                    cr, cp = parsed_function_by_name[cn]
                    nested.append(f"  {cr} {cn} ({cp})")
            scope_entries.append((display, None, None, nested or None))

        for fe_sig, name, params, body in sorted(foreach_macro_entries, key=lambda e: e[0]):
            display = fe_sig
            nested: list[str] = []
            if params is not None:
                raw = split_top_level_commas(params)
                parts = [p for p in raw if p != "..."]
                varargs = len(parts) != len(raw)
                calls = [f"{name}({params})"]
                if varargs:
                    calls.append(f"{name}({', '.join(parts)})")
                nested = ["Caller:"]
                for c in calls:
                    nested.append(f"  {c}")
            else:
                nested = ["Caller:", f"  {name}"]
            foreach_entries.append((display, None, None, nested or None))

    all_type_names = sorted(set(doc.typedefs) | set(doc.struct_fields.keys()))
    renderable_macros: list[str] = []
    for macro in doc.macros:
        if macro.startswith(f"{_ns.snake}_") or macro.startswith(f"{_ns.upper}_"):
            continue
        name, _ = parse_macro_signature(macro)
        if name.endswith("_scope") or name.endswith("_foreach"):
            continue
        if macro in resolved_macro_wrappers:
            _, p = parse_macro_signature(macro)
            if p and "..." in p:
                continue
        renderable_macros.append(macro)

    return ModuleSections(
        parsed_decls=parsed_decls,
        parsed_function_by_name=parsed_function_by_name,
        opt_entries=opt_entries,
        scope_entries=scope_entries,
        foreach_entries=foreach_entries,
        resolved_macro_wrappers=resolved_macro_wrappers,
        all_type_names=all_type_names,
        renderable_macros=renderable_macros,
    )


def resolve_param_doc(param_desc: str, typedef_docs, struct_fields):
    """Returns ('inherit', [field_lines]) or ('normal', [])."""
    from .parsing import parse_doc_comment
    m = re.match(r"^@inherit\((\w+)\)", param_desc)
    if not m:
        return "normal", []
    inh = m.group(1)
    inh_doc = typedef_docs.get(inh, "")
    info = parse_doc_comment(inh_doc) if inh_doc else None
    fields = struct_fields.get(inh, [])
    out: list[str] = []
    if fields:
        mw = max(len(f".{fn}: ") for _, fn in fields)
        for _, fn in fields:
            fd = info.params.get(fn, "") if info else ""
            label = f".{fn}: "
            out.append(f"  {label:<{mw}}{fd}".rstrip())
    elif info and info.params:
        for pn, pd in info.params.items():
            out.append(f"  .{pn}: {pd}")
    return "inherit", out
