"""cflatdoc — generate man pages, HTML, and JSON docs from Cflat headers."""
from .model import DocInfo, EnumDoc, ModuleDoc
from .parsing import parse_header
from .render_roff import render_module_roff, render_index_roff
from .render_html import render_module_html, render_index_html
from .render_json import dump_modules_json

__all__ = [
    "DocInfo", "EnumDoc", "ModuleDoc",
    "parse_header",
    "render_module_roff", "render_index_roff",
    "render_module_html", "render_index_html",
    "dump_modules_json",
]
