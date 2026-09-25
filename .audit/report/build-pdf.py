#!/usr/bin/env python3
"""WCAP report PDF: findings data -> Jinja2 HTML -> headless Chrome -> PDF (WBP-1999 / WBP-2003).

Usage:
  .audit/report/build-pdf.py <data> <out.pdf> [--template template.html] [--meta report-meta.yaml]
                             [--theme theme.css] [--chrome google-chrome]

<data> is either the JSON emitted by `render.py --report-data` (the production path: the findings
YAML is the single source of truth, so the PDF cannot drift from it) or a small hand-written YAML in
the proof-of-concept shape (see sample-findings.yaml, which is what CI and the smoke test use).

The findings JSON and the rendered PDF are PRIVATE until the findings are fixed and deployed:
write them to .audit/report/out/ (gitignored) or outside the repository, never commit them here.
Only the template, theme, fonts, logo and non-sensitive metadata live in this directory.

Paths for the template, theme, metadata, fonts and logo are resolved relative to this script, so it
runs from any working directory.
"""
import argparse, base64, json, pathlib, subprocess, sys

try:
    import jinja2, yaml
except ImportError:
    sys.exit("Jinja2 and PyYAML required: pip install jinja2 pyyaml")

HERE = pathlib.Path(__file__).resolve().parent

ap = argparse.ArgumentParser()
ap.add_argument("data"); ap.add_argument("out")
ap.add_argument("--template", default=HERE / "template.html", type=pathlib.Path)
ap.add_argument("--meta", default=HERE / "report-meta.yaml", type=pathlib.Path)
ap.add_argument("--theme", default=HERE / "theme.css", type=pathlib.Path)
ap.add_argument("--chrome", default="google-chrome")
a = ap.parse_args()

SEV = ["critical", "high", "medium", "low", "info"]

src = pathlib.Path(a.data)
if src.suffix == ".json":                                 # render.py --report-data
    rd = json.loads(src.read_text())
    meta = yaml.safe_load(a.meta.read_text())
    data = {
        "audit": {**meta, "provenance": rd["provenance"], "protocol": rd.get("protocol", "WCAP v1"),
                  "generated_at": rd.get("generated_at", "")},
        "findings": [f for f in rd["findings"] if f["severity"] != "info"],
        "informational": [f for f in rd["findings"] if f["severity"] == "info"],
        "counts": {s: rd["counts"]["by_severity"].get(s, 0) for s in SEV},
        # status counts over the security findings only, to match the line they sit under;
        # informational items carry their own status in their own section
        "by_status": {k: sum(1 for f in rd["findings"] if f["severity"] != "info" and f["status"] == k)
                      for k in ("fixed", "open")},
        "source": "render.py --report-data",
    }
else:                                                     # proof-of-concept YAML
    y = yaml.safe_load(src.read_text())
    data = {"audit": y["audit"], "findings": y.get("findings", []), "informational": [],
            "counts": {s: sum(1 for f in y.get("findings", []) if f["severity"] == s) for s in SEV},
            "by_status": {}, "source": src.name}

OPTIONAL = ("invariant", "asset", "impact", "likelihood", "attack_scenario", "proof", "remediation",
            "remediation_notes", "notes", "location", "class")
for f in data["findings"] + data["informational"]:          # StrictUndefined: be explicit about absence
    for k in OPTIONAL:
        f.setdefault(k, [] if k == "location" else None)

footer_left = f'{data["audit"].get("id", "")} \\00b7 {data["audit"]["scope"]["repo"]} @ {data["audit"]["scope"]["commit"]}'.replace('"', '')
data["theme_css"] = a.theme.read_text().replace("FOOTER_LEFT", footer_left)

# Fonts are self-hosted so the PDF is reproducible offline. The rendered HTML is written next to the
# PDF, not next to this script, so the relative url('fonts/…') in the CSS is embedded as data URIs.
fonts_css = HERE / "fonts" / "opensans-local.css"
data["font_css"] = ""
if fonts_css.exists():
    css = fonts_css.read_text()
    for ttf in sorted((HERE / "fonts").glob("*.ttf")):
        b64 = base64.b64encode(ttf.read_bytes()).decode()
        css = css.replace(f"url('fonts/{ttf.name}')", f"url('data:font/ttf;base64,{b64}')")
    data["font_css"] = css
logo = HERE / "logo.svg"
data["logo_svg"] = logo.read_text() if logo.exists() else ""

sc = data["audit"].get("scores", [])
data["overall"] = round(sum(s["score"] * s["weight"] for s in sc) / sum(s["weight"] for s in sc), 1) if sc else None

env = jinja2.Environment(loader=jinja2.FileSystemLoader(str(a.template.resolve().parent)), autoescape=True,
                         undefined=jinja2.StrictUndefined)
html = env.get_template(a.template.name).render(**data)
out = pathlib.Path(a.out)
out.parent.mkdir(parents=True, exist_ok=True)
html_path = out.with_suffix(".html")
html_path.write_text(html)
cmd = [a.chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
       f"--print-to-pdf={out}", str(html_path.resolve())]
r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
if r.returncode or not out.exists():
    sys.exit(f"chrome failed: {r.stderr[-800:]}")
print(f"wrote {out} from {data['source']}")
