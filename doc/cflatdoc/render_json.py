"""JSON serializer — emits a single docs.json the web viewer consumes."""
from __future__ import annotations
import json
from dataclasses import asdict
from .model import ModuleDoc
from .parsing import parse_doc_comment
from .sections import collect_module_sections


def _info_dict(text: str) -> dict:
    if not text:
        return {}
    info = parse_doc_comment(text)
    return {
        "description": info.description,
        "params": info.params,
        "returns": info.returns,
        "preconditions": info.preconditions,
        "postconditions": info.postconditions,
        "invariants": info.invariants,
    }


def _module_json(doc: ModuleDoc) -> dict:
    sec = collect_module_sections(doc)
    functions = []
    for fn, ret, params, decl, _ in sec.parsed_decls:
        functions.append({
            "name": fn,
            "returnType": ret,
            "params": params,
            "decl": decl,
            "doc": _info_dict(doc.declaration_docs.get(decl, "")),
        })

    types = []
    for tn in sec.all_type_names:
        types.append({
            "name": tn,
            "doc": _info_dict(doc.typedef_docs.get(tn, "")),
            "fields": [
                {"type": ft, "name": fn} for ft, fn in doc.struct_fields.get(tn, [])
            ],
        })

    enums = []
    for e in doc.enums:
        members = []
        for m in e.members:
            n = m.split("=", 1)[0].strip()
            v = m.split("=", 1)[1].strip() if "=" in m else None
            members.append({"name": n, "value": v})
        enums.append({"name": e.name, "baseType": e.base_type,
                      "doc": e.doc, "members": members})

    macros = []
    for macro in sec.renderable_macros:
        wrap = sec.resolved_macro_wrappers.get(macro)
        macros.append({
            "signature": macro,
            "value": doc.macro_values.get(macro),
            "wrapper": ({"target": wrap[0], "defaults": wrap[1]} if wrap else None),
            "doc": doc.macro_docs.get(macro, ""),
        })

    def _entries(es):
        out = []
        for name, ret, params, nested in es:
            out.append({"name": name, "returnType": ret, "params": params,
                        "nested": list(nested) if nested else []})
        return out

    return {
        "slug": doc.module_name,
        "header": doc.header_name,
        "title": doc.title,
        "synopsis": "\n".join(doc.synopsis_comments).strip(),
        "functions": functions,
        "types": types,
        "enums": enums,
        "macros": macros,
        "opts": _entries(sec.opt_entries),
        "scopes": _entries(sec.scope_entries),
        "foreach": _entries(sec.foreach_entries),
    }


def dump_modules_json(modules: list[ModuleDoc], synopsis: list[str]) -> str:
    payload = {
        "synopsis": "\n".join(synopsis).strip(),
        "modules": [_module_json(m) for m in modules],
    }
    return json.dumps(payload, indent=2)
