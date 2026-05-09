"""Plain data classes describing a parsed Cflat header."""
from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class DocInfo:
    description: str = ""
    params: dict[str, str] = field(default_factory=dict)
    returns: str = ""
    preconditions: list[str] = field(default_factory=list)
    postconditions: list[str] = field(default_factory=list)
    invariants: list[str] = field(default_factory=list)


@dataclass
class EnumDoc:
    name: str
    base_type: str
    signature: str
    members: list[str]
    doc: str = ""


@dataclass
class ModuleDoc:
    module_name: str
    header_name: str
    title: str
    synopsis_comments: list[str] = field(default_factory=list)
    typedefs: list[str] = field(default_factory=list)
    declarations: list[str] = field(default_factory=list)
    declaration_docs: dict[str, str] = field(default_factory=dict)
    macros: list[str] = field(default_factory=list)
    macro_bodies: dict[str, str] = field(default_factory=dict)
    macro_values: dict[str, str] = field(default_factory=dict)
    macro_opt_wrappers: dict[str, tuple[str, dict[str, str]]] = field(default_factory=dict)
    opt_struct_fields: dict[str, list[tuple[str, str]]] = field(default_factory=dict)
    struct_fields: dict[str, list[tuple[str, str]]] = field(default_factory=dict)
    macro_docs: dict[str, str] = field(default_factory=dict)
    typedef_docs: dict[str, str] = field(default_factory=dict)
    enums: list[EnumDoc] = field(default_factory=list)
    struct_tags: dict[str, str] = field(default_factory=dict)
    backed_by_fields_macro: dict[str, list[str]] = field(default_factory=dict)


@dataclass
class ModuleSections:
    parsed_decls: list
    parsed_function_by_name: dict
    opt_entries: list
    scope_entries: list
    foreach_entries: list
    resolved_macro_wrappers: dict
    all_type_names: list
    renderable_macros: list
