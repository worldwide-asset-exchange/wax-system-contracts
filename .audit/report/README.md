# `.audit/report/` — the audit report PDF pipeline

Renders the WCAP audit report as a WAX-branded PDF from data, so the document is generated and
never authored by hand (WBP-1999 research, WBP-2003 delivery). If someone edits the PDF, the next
render overwrites it; that is the point — the two earlier in-house audits drifted because each
artefact was written separately.

```
findings data ──► build-pdf.py ──► Jinja2 (template.html + theme.css) ──► headless Chrome ──► PDF
```

## What is here

| Path | Purpose |
|---|---|
| `build-pdf.py` | The single command. Data in, PDF out. Needs `python3`, `jinja2`, `pyyaml`, `google-chrome`. |
| `template.html` | Jinja2 template: cover, scope, provenance, scope and method (when `method` is present), severity tiles, scorecard with basis and rubric (when present), findings, informational items, disclaimer. |
| `theme.css` | WAX brand tokens (colours from wax.io/branding), A4 page setup, running footer. |
| `report-meta.yaml` | Non-sensitive metadata the data file does not carry: title, client, scope, the scorecard with a one-line `basis` per dimension, the `rubric` the scores are read on, and the `method` block (window, summary, WCAP phases, review depth by surface, limits) that renders as the "Scope and method" section. Keep `scores`/`basis` in step with the Confluence scorecard. An `editions:` map holds what a named edition changes (see **Editions**). |
| `logo.svg` | WAX word mark for the cover. |
| `fonts/` | Open Sans 400/600/700, self-hosted so the PDF is reproducible offline. Licence: `fonts/OFL.txt`. |
| `sample-findings.yaml` | Illustrative data in the proof-of-concept shape. Not a real finding; exercises the template. |

## What is deliberately *not* here

The findings themselves. The production data file is the JSON emitted by the private
`render.py --report-data` (findings YAML is the single source of truth, kept outside this
repository — see [`../README.md`](../README.md)). That JSON and the rendered PDF stay private
until the findings are fixed and deployed, and are never committed here: `.audit/report/out/` and
any `*.json` / `*.pdf` under this directory are gitignored.

## Usage

```bash
# smoke test with the illustrative sample (safe to run anywhere, including CI)
.audit/report/build-pdf.py .audit/report/sample-findings.yaml .audit/report/out/sample.pdf

# the real report, from the private findings JSON (kept outside the repo, mode 600)
.audit/report/build-pdf.py /path/to/report-data.json /path/to/private/wcap-report.pdf
```

The rendered HTML is written next to the PDF (same name, `.html`) for inspection.

## Editions

One report can be published to more than one audience. `--profile <name>` renders the edition
named under `editions:` in `report-meta.yaml`: its block is merged over the top-level metadata
(dicts recursively; lists of records such as `scores` and `method.phases` by their `dimension` /
`phase` key), so an edition restates only the entries it changes — a different `basis`, an
`edition` label on the cover, a `scope_statement` callout. The default profile, `internal`, is
the file as is. Which findings and which fields an edition carries is decided where the data is
rendered (`render.py --profile`), and the JSON records the profile it was rendered for;
`build-pdf.py` refuses to combine one edition's metadata with another edition's data.

```bash
.audit/report/build-pdf.py /path/private/report-data-external.json /path/private/report-external.pdf --profile external
```

## Provenance

The provenance statement is carried by the data file, not the template, and is rendered verbatim
both in the callout on page 1 and in the closing section. The report is an internal assessment and
says so in its running footer and disclaimer; it is not a third-party attestation.
