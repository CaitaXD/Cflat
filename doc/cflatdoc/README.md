# cflatdoc

Generate man pages, HTML docs, and JSON from Cflat C headers.

A clean modular rewrite of the original 1800-line `generate_man_pages.py`.

## Usage

```bash
python -m cflatdoc                           # use repo defaults
python -m cflatdoc --src-dir src --html-dir docs --json-dir docs
```

Outputs:
- `<out-dir>/cflat-*.3` — roff man pages
- `<html-dir>/*.html` — beautiful self-contained HTML (sidebar + dark mode + ⌘K search)
- `<json-dir>/docs.json` — machine-readable dump that powers the Lovable web viewer

## Layout

```
cflatdoc/
  __init__.py
  __main__.py
  cli.py            # argparse + I/O orchestration
  model.py          # dataclasses: DocInfo, EnumDoc, ModuleDoc, ModuleSections
  parsing.py        # header → ModuleDoc (regex parsers)
  sections.py       # ModuleDoc → renderable sections (opts/scopes/foreach/macros)
  render_roff.py    # man pages
  render_html.py    # standalone HTML (with C tokenizer + markdown)
  render_json.py    # docs.json
  assets/
    style.css
    app.js          # theme toggle, search, copy buttons
```

The parsing regexes are preserved verbatim from the original — the rewrite only
relocates them, gives them clean names, and replaces the renderers.

## Web viewer

Drop the generated `docs.json` into the companion Lovable app at
`public/docs.json` for an even nicer interactive viewer with deep linking and
fuzzy search across every symbol.
