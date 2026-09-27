#!/usr/bin/env python3
"""
compare_tinyusb.py
------------------
Vergleicht die C-/Header-Dateien des TinyUSB-Vendoring im Rhythm-Projekt
(firmware/vendor/tusb/src) gegen das lokale TinyUSB-Repo
(C:\\Users\\Philipp\\Projekte\\Demo\\tinyusb\\src) und erzeugt einen
übersichtlichen HTML-Report.

Report-Aufbau:
  1) Statistik  – Anzahl C/H-Dateien, identisch, mit Unterschieden,
                  nur in Rhythm, nur im Repo, + wichtige Checks
                  (TinyUSB-Versionsnummer, Git-Commit des Repos,
                  diff-Größe je Datei).
  2) Unterschiede – für jede sich unterscheidende Datei: Pfad,
                  Größe-Δ, Zeilen-Δ, plus ein Inline-Unified-Diff.
  3) Alle verglichenen Dateien – komplette Tabelle mit Status.

Aufruf:
    python compare_tinyusb.py
    (optionale Pfade als 1./2. Argument übergeben)

Kein Schreibzugriff auf das Rhythm-Projekt – reine Lese-Operation.
"""

import os
import sys
import hashlib
import datetime
import difflib
import html
import subprocess
import json

# ---------------------------------------------------------------------------
# Pfade (standardmäßig die des Rhythm-Projekts, überschreibbar)
# ---------------------------------------------------------------------------
PROJECT_TUSB = r"C:\Users\Philipp\Projekte\rhythm\firmware\vendor\tusb\src"
REPO_TUSB    = r"C:\Users\Philipp\Projekte\Demo\tinyusb\src"
REPORT_PATH  = r"C:\Users\Philipp\Projekte\rhythm\docs\tinyusb_diff_report.html"
REPO_ROOT    = r"C:\Users\Philipp\Projekte\Demo\tinyusb"

if len(sys.argv) >= 4:
    PROJECT_TUSB, REPO_TUSB, REPORT_PATH = sys.argv[1], sys.argv[2], sys.argv[3]
elif len(sys.argv) == 3:
    PROJECT_TUSB, REPO_TUSB = sys.argv[1], sys.argv[2]
elif len(sys.argv) == 2:
    REPORT_PATH = sys.argv[1]

EXTS = {".c", ".h"}


# ---------------------------------------------------------------------------
# Hilfsfunktionen
# ---------------------------------------------------------------------------
def sha256_of(path):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return None


def rel_lines(a, b):
    """Zeilenlisten lesen (fehlgeschlagene Dateien -> [])."""
    for p in (a, b):
        pass
    out = []
    for p in (a, b):
        try:
            with open(p, "r", encoding="utf-8", errors="replace") as f:
                out.append(f.read().splitlines())
        except OSError:
            out.append([])
    return out[0], out[1]


def unified_diff(a, b, name_a, name_b, context=3, max_lines=400):
    """Unified-Diff, auf max_lines Zeilen gekürzt (mit Hinweis)."""
    diff = list(difflib.unified_diff(a, b, fromfile=name_a, tofile=name_b,
                                     n=context, lineterm=""))
    truncated = False
    if len(diff) > max_lines:
        diff = diff[:max_lines]
        truncated = True
    return diff, truncated


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def extract_tinyusb_version(repo_root):
    """
    TinyUSB-Version aus dem Repo bestimmen.
    TinyUSB hat kein #define-CFG_TUSB_VERSION – die Version steht in
    version.yml (repo.version) bzw. im Git-Tag des Commits.
    """
    if not repo_root:
        return None
    # 1) version.yml
    vfile = os.path.join(repo_root, "version.yml")
    try:
        with open(vfile, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("repo.version:"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    # 2) Git-Tag des aktuellen HEAD
    try:
        tag = subprocess.check_output(
            ["git", "-C", repo_root, "describe", "--tags", "--exact-match"],
            stderr=subprocess.DEVNULL).decode().strip()
        if tag:
            return tag
    except Exception:
        pass
    return None


def repo_git_info(root):
    """Git-Commit + Branch des lokalen TinyUSB-Repo."""
    info = {"commit": None, "branch": None, "remote": None}
    if not os.path.isdir(os.path.join(root, ".git")):
        return info
    try:
        info["commit"] = subprocess.check_output(
            ["git", "-C", root, "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        pass
    try:
        info["branch"] = subprocess.check_output(
            ["git", "-C", root, "rev-parse", "--abbrev-ref", "HEAD"],
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        pass
    try:
        info["remote"] = subprocess.check_output(
            ["git", "-C", root, "remote", "get-url", "origin"],
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        pass
    return info


# ---------------------------------------------------------------------------
# Hauptvergleiche
# ---------------------------------------------------------------------------
def collect_c_files(root):
    """Alle .c/.h unter root, relativ zum root, als dict {relpath: abspath}."""
    found = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext in EXTS:
                absp = os.path.join(dirpath, fn)
                rel = os.path.relpath(absp, root).replace(os.sep, "/")
                found[rel] = absp
    return found


def main():
    now = datetime.datetime.now().strftime("%d.%m.%Y %H:%M:%S")

    project_files = collect_c_files(PROJECT_TUSB)
    repo_files    = collect_c_files(REPO_TUSB)

    project_set = set(project_files)
    repo_set    = set(repo_files)
    common      = sorted(project_set & repo_set)
    only_project = sorted(project_set - repo_set)
    only_repo    = sorted(repo_set - project_set)

    identical = []          # relpath
    differing = []          # dict mit Detail
    errors    = []

    for rel in common:
        p_proj = project_files[rel]
        p_repo = repo_files[rel]

        h1 = sha256_of(p_proj)
        h2 = sha256_of(p_repo)

        if h1 is None or h2 is None:
            errors.append(rel)
            continue

        size_proj = os.path.getsize(p_proj)
        size_repo = os.path.getsize(p_repo)

        if h1 == h2:
            identical.append(rel)
            continue

        lines_proj, lines_repo = rel_lines(p_proj, p_repo)
        diff, truncated = unified_diff(lines_proj, lines_repo,
                                       "rhythm/" + rel, "repo/" + rel)
        # Anzahl +/−
        added = sum(1 for d in diff if d.startswith("+") and not d.startswith("+++"))
        removed = sum(1 for d in diff if d.startswith("-") and not d.startswith("---"))

        differing.append({
            "rel": rel,
            "size_proj": size_proj,
            "size_repo": size_repo,
            "size_delta": size_proj - size_repo,
            "lines_proj": len(lines_proj),
            "lines_repo": len(lines_repo),
            "added": added,
            "removed": removed,
            "diff": diff,
            "truncated": truncated,
            "sha_proj": h1,
            "sha_repo": h2,
        })

    # Versionen (Repo hat version.yml; Rhythm hat kein version.yml → Git-Tag)
    ver_repo = extract_tinyusb_version(REPO_ROOT)
    ver_proj = None  # Rhythm hat kein version.yml
    try:
        ver_proj = subprocess.check_output(
            ["git", "-C", r"C:\Users\Philipp\Projekte\rhythm", "describe",
             "--tags", "--match", "*tinyusb*"],
            stderr=subprocess.DEVNULL).decode().strip() or None
    except Exception:
        pass
    git_info = repo_git_info(REPO_ROOT)

    # ------------------------------------------------------------------
    # HTML-Report
    # ------------------------------------------------------------------
    total = len(common) + len(only_project) + len(only_repo)
    pct_ident = (len(identical) / total * 100) if total else 0.0

    def badge(text, cls):
        return '<span class="badge {}">{}</span>'.format(cls, html.escape(text))

    def status_badge(kind):
        return {
            "ident":  badge("identisch", "b-ok"),
            "diff":   badge("unterschiedlich", "b-warn"),
            "proj":   badge("nur in Rhythm", "b-info"),
            "repo":   badge("nur im Repo", "b-muted"),
        }[kind]

    # ---------- Header ----------
    html_parts = []
    html_parts.append("""<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>TinyUSB Diff Report</title>
<style>
  :root {
    --bg: #1a1a2e; --panel: #16213e; --border: #0f3460;
    --text: #e8e8e8; --muted: #8892b0;
    --ok: #4ade80; --warn: #fbbf24; --info: #60a5fa; --muted2: #9ca3af;
    --add: #22c55e; --rem: #ef4444;
  }
  * { box-sizing: border-box; }
  body {
    background: var(--bg); color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    margin: 0; padding: 24px; font-size: 14px;
  }
  h1 { font-size: 22px; margin: 0 0 4px; }
  .subtitle { color: var(--muted); margin-bottom: 20px; }
  .panel {
    background: var(--panel); border: 1px solid var(--border);
    border-radius: 8px; padding: 16px; margin-bottom: 20px;
  }
  .panel h2 { font-size: 16px; margin: 0 0 12px; border-bottom: 1px solid var(--border); padding-bottom: 8px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 10px; margin-bottom: 12px; }
  .stat {
    background: rgba(255,255,255,0.03); border: 1px solid var(--border);
    border-radius: 6px; padding: 10px 12px; text-align: center;
  }
  .stat .num { font-size: 26px; font-weight: 700; display: block; }
  .stat .lbl { color: var(--muted); font-size: 12px; }
  .badge {
    display: inline-block; padding: 2px 8px; border-radius: 4px;
    font-size: 11px; font-weight: 600; white-space: nowrap;
  }
  .b-ok    { background: rgba(74,222,128,.15); color: var(--ok);   border: 1px solid var(--ok); }
  .b-warn  { background: rgba(251,191,36,.15); color: var(--warn); border: 1px solid var(--warn); }
  .b-info  { background: rgba(96,165,250,.15); color: var(--info); border: 1px solid var(--info); }
  .b-muted { background: rgba(156,163,175,.15);color: var(--muted2);border: 1px solid var(--muted2); }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th, td { text-align: left; padding: 6px 10px; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-size: 11px; text-transform: uppercase; letter-spacing: .5px; }
  tr:hover td { background: rgba(255,255,255,.03); }
  .checks { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 10px; }
  .check {
    display: flex; align-items: center; gap: 8px;
    background: rgba(255,255,255,.03); border: 1px solid var(--border);
    border-radius: 5px; padding: 8px 12px; font-size: 13px;
  }
  .check .icon { font-size: 16px; }
  .check .val  { margin-left: auto; font-weight: 600; font-family: monospace; font-size: 12px; }
  /* Diff-Anzeige */
  .diff-block {
    background: #0d1117; border: 1px solid var(--border); border-radius: 6px;
    padding: 10px; margin: 8px 0 16px; overflow-x: auto; max-height: 420px; overflow-y: auto;
    font-family: 'Consolas', monospace; font-size: 12px; line-height: 1.5;
  }
  .diff-block div { white-space: pre; }
  .diff-add { color: var(--add); }
  .diff-rem { color: var(--rem); }
  .diff-hdr { color: var(--info); font-weight: 600; }
  .diff-met { color: var(--muted); margin: 4px 0 2px; font-size: 12px; }
  .diff-met code { background: rgba(255,255,255,.06); padding: 1px 5px; border-radius: 3px; font-size: 11px; }
  .trunc-note { color: var(--warn); font-size: 11px; margin-top: 4px; }
  .file-row { border-bottom: 1px solid var(--border); padding: 8px 0; }
  .file-row:last-child { border-bottom: none; }
  .empty-note { color: var(--muted); font-style: italic; padding: 8px 0; }
  .progress-bar { height: 6px; background: var(--border); border-radius: 3px; overflow: hidden; margin-top: 4px; }
  .progress-fill { height: 100%; background: var(--ok); }
  .footer { color: var(--muted); font-size: 11px; margin-top: 20px; text-align: center; }
  details { margin: 4px 0; }
  summary { cursor: pointer; color: var(--info); font-size: 13px; }
</style>
</head>
<body>
""")

    # ---------- Kopf ----------
    html_parts.append(f"""
<h1>TinyUSB Diff Report – Rhythm Projekt</h1>
<div class="subtitle">
  Rhythm-Vendoring: <code>{html.escape(PROJECT_TUSB)}</code><br>
  Referenz-Repo: <code>{html.escape(REPO_TUSB)}</code><br>
  Erstellt: {now}
</div>
""")

    # ---------- Panel 1: Statistik ----------
    html_parts.append('<div class="panel"><h2>📊 Statistik</h2>')
    html_parts.append('<div class="grid">')
    for num, lbl, cls in [
        (total,          "Verglichen (C+H)", ""),
        (len(identical), "Identisch",        "ok"),
        (len(differing), "Unterschiedlich",  "warn" if differing else "ok"),
        (len(only_project), "Nur in Rhythm", "info" if only_project else "ok"),
        (len(only_repo),    "Nur im Repo",   "muted" if only_repo else "ok"),
    ]:
        color = {"ok": "var(--ok)", "warn": "var(--warn)", "info": "var(--info)",
                 "muted": "var(--muted2)"}.get(cls, "var(--text)")
        html_parts.append(
            f'<div class="stat"><span class="num" style="color:{color}">{num}</span>'
            f'<span class="lbl">{lbl}</span></div>')
    html_parts.append('</div>')

    # Fortschrittsbalken
    html_parts.append(
        f'<div style="font-size:12px;color:var(--muted);margin-bottom:4px">'
        f'Identische Dateien: {pct_ident:.1f}%</div>')
    html_parts.append(
        f'<div class="progress-bar"><div class="progress-fill" '
        f'style="width:{pct_ident:.1f}%"></div></div>')

    # Wichtige Checks
    html_parts.append('<div class="checks">')
    checks = []
    # Version
    if ver_proj and ver_repo:
        match = ver_proj == ver_repo
        icon = "✅" if match else "⚠️"
        checks.append((icon, "TinyUSB-Version", f"{ver_proj} vs {ver_repo}"))
    else:
        checks.append(("❓", "TinyUSB-Version", "nicht erkannt"))
    # Git
    if git_info["commit"]:
        short = git_info["commit"][:8]
        checks.append(("🔀", "Repo-Commit", f"{short} ({git_info.get('branch','?')})"))
    else:
        checks.append(("❓", "Repo-Commit", "kein Git-Repo"))
    # Nur in Rhythm
    if only_project:
        checks.append(("⚠️", "Nur in Rhythm", f"{len(only_project)} Datei(en)"))
    else:
        checks.append(("✅", "Nur in Rhythm", "keine"))
    # Nur im Repo
    if only_repo:
        checks.append(("ℹ️", "Nur im Repo", f"{len(only_repo)} Datei(en)"))
    else:
        checks.append(("✅", "Nur im Repo", "keine"))
    # Summe diff Zeilen
    tot_add = sum(d["added"] for d in differing)
    tot_rem = sum(d["removed"] for d in differing)
    if differing:
        checks.append(("📝", "Diff-Zeilen (Σ)", f"+{tot_add} / −{tot_rem}"))
    else:
        checks.append(("✅", "Diff-Zeilen (Σ)", "keine"))
    # Fehler
    if errors:
        checks.append(("❌", "Fehler beim Lesen", f"{len(errors)} Datei(en)"))
    else:
        checks.append(("✅", "Lesefehler", "keine"))

    for icon, lbl, val in checks:
        html_parts.append(
            f'<div class="check"><span class="icon">{icon}</span>'
            f'<span>{lbl}</span><span class="val">{html.escape(val)}</span></div>')
    html_parts.append('</div></div>')

    # ---------- Panel 2: Unterschiede ----------
    html_parts.append('<div class="panel"><h2>🔍 Unterscheidende Dateien</h2>')
    if not differing:
        html_parts.append('<div class="empty-note">Keine Unterschiede gefunden – '
                          'alle gemeinsamen Dateien sind identisch. 🎉</div>')
    else:
        for d in differing:
            rel = d["rel"]
            delta_size = d["size_delta"]
            size_str = f"{d['size_proj']} B → {d['size_repo']} B"
            if delta_size != 0:
                sign = "+" if delta_size > 0 else ""
                size_str += f' ({sign}{delta_size} B)'
            html_parts.append(f'<div class="file-row">')
            html_parts.append(
                f'<div class="diff-met">'
                f'<code>{html.escape(rel)}</code><br>'
                f'{status_badge("diff")} &nbsp; '
                f'Größe: {size_str} &nbsp; '
                f'Zeilen: {d["lines_proj"]} → {d["lines_repo"]} &nbsp; '
                f'<span style="color:var(--add)">+{d["added"]}</span> / '
                f'<span style="color:var(--rem)">−{d["removed"]}</span>'
                f'</div>')
            html_parts.append('<div class="diff-block">')
            for line in d["diff"]:
                if line.startswith("+++") or line.startswith("---"):
                    cls = "diff-hdr"
                elif line.startswith("@@"):
                    cls = "diff-hdr"
                elif line.startswith("+"):
                    cls = "diff-add"
                elif line.startswith("-"):
                    cls = "diff-rem"
                else:
                    cls = ""
                esc = html.escape(line) if line else "&nbsp;"
                if cls:
                    html_parts.append(f'<div class="{cls}">{esc}</div>')
                else:
                    html_parts.append(f'<div>{esc}</div>')
            html_parts.append('</div>')
            if d["truncated"]:
                html_parts.append(
                    f'<div class="trunc-note">⚠️ Diff gekürzt auf '
                    f'{len(d["diff"])} Zeilen – vollständiger Diff per '
                    f'<code>git diff</code> im Repo.</div>')
            html_parts.append('</div>')

    # Nur in Rhythm / nur im Repo – kompakt
    if only_project or only_repo:
        html_parts.append('<div style="margin-top:12px">')
        if only_project:
            html_parts.append(f'<div style="color:var(--info);margin-bottom:4px;font-size:13px">'
                              f'Nur in Rhythm ({len(only_project)}):</div>')
            for rel in only_project:
                html_parts.append(f'<div style="font-family:monospace;font-size:12px;padding:2px 0">'
                                  f'&nbsp;&nbsp;{status_badge("proj")} {html.escape(rel)}</div>')
        if only_repo:
            html_parts.append(f'<div style="color:var(--muted);margin:8px 0 4px;font-size:13px">'
                              f'Nur im Repo ({len(only_repo)}) – nicht vendored:</div>')
            for rel in only_repo:
                html_parts.append(f'<div style="font-family:monospace;font-size:12px;padding:2px 0">'
                                  f'&nbsp;&nbsp;{status_badge("repo")} {html.escape(rel)}</div>')
        html_parts.append('</div>')
    html_parts.append('</div>')

    # ---------- Panel 3: Alle verglichenen Dateien ----------
    html_parts.append('<div class="panel"><h2>📋 Alle verglichenen Dateien</h2>')
    html_parts.append('<table><thead><tr>'
                      '<th>Datei</th><th>Status</th>'
                      '<th>Größe (Rhythm)</th><th>Größe (Repo)</th>'
                      '</tr></thead><tbody>')

    # Sortierung: erst identisch, dann diff, dann nur-in-rhythm, dann nur-im-repo
    def sort_key(rel):
        if rel in set(d["rel"] for d in differing): return (0, rel)
        if rel in set(only_project):               return (1, rel)
        if rel in set(only_repo):                  return (2, rel)
        return (3, rel)

    all_rels = sorted(common + only_project + only_repo, key=sort_key)

    diff_rels = set(d["rel"] for d in differing)
    proj_rels = set(only_project)
    repo_rels = set(only_repo)

    for rel in all_rels:
        if rel in proj_rels:
            st = status_badge("proj")
            size_p = os.path.getsize(project_files[rel]) if rel in project_files else 0
            size_r = "–"
            size_p_s = f"{size_p} B"
        elif rel in repo_rels:
            st = status_badge("repo")
            size_p = "–"
            size_r = f"{os.path.getsize(repo_files[rel])} B"
            size_p_s = "–"
        elif rel in diff_rels:
            st = status_badge("diff")
            dd = next(d for d in differing if d["rel"] == rel)
            size_p_s = f"{dd['size_proj']} B"
            size_r = f"{dd['size_repo']} B"
        else:
            st = status_badge("ident")
            size_p = os.path.getsize(project_files[rel])
            size_p_s = f"{size_p} B"
            size_r = size_p_s

        html_parts.append(
            f'<tr><td style="font-family:monospace;font-size:12px">'
            f'{html.escape(rel)}</td><td>{st}</td>'
            f'<td>{size_p_s}</td><td>{size_r}</td></tr>')

    html_parts.append('</tbody></table>')
    html_parts.append('</div>')

    # Footer
    html_parts.append(f"""
<div class="footer">
  TinyUSB Diff Report · {now} · {len(common)} gemeinsame,
  {len(identical)} identisch, {len(differing)} unterschiedlich,
  {len(only_project)} nur in Rhythm, {len(only_repo)} nur im Repo
</div>
</body></html>
""")

    html_doc = "\n".join(html_parts)

    # Schreiben
    out_dir = os.path.dirname(REPORT_PATH)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write(html_doc)

    # Konsole-Zusammenfassung
    print(f"\n{'='*60}")
    print("  TinyUSB Diff Report")
    print(f"{'='*60}")
    print(f"  Rhythm-Vendoring : {PROJECT_TUSB}")
    print(f"  Referenz-Repo    : {REPO_TUSB}")
    print(f"{'-'*60}")
    print(f"  Gemeinsame Dateien (C+H) : {len(common)}")
    print(f"    identisch               : {len(identical)}")
    print(f"    unterschiedlich         : {len(differing)}")
    print(f"  Nur in Rhythm             : {len(only_project)}")
    print(f"  Nur im Repo               : {len(only_repo)}")
    if ver_proj:
        print(f"  TinyUSB-Version Rhythm    : {ver_proj}")
    if ver_repo:
        print(f"  TinyUSB-Version Repo      : {ver_repo}")
    if git_info["commit"]:
        print(f"  Repo-Commit               : {git_info['commit'][:12]} ({git_info.get('branch','?')})")
    print(f"{'-'*60}")
    if differing:
        print(f"\n  Unterschiedliche Dateien:")
        for d in differing:
            print(f"    ✏️  {d['rel']}  (+{d['added']}/-{d['removed']} Zeilen)")
    if only_project:
        print(f"\n  Nur in Rhythm:")
        for rel in only_project:
            print(f"    ➕ {rel}")
    if only_repo:
        print(f"\n  Nur im Repo (nicht vendored): {len(only_repo)} Dateien")
    print(f"{'-'*60}")
    print(f"  Report: {REPORT_PATH}")
    print(f"{'='*60}\n")

    # Rückgabewert: 0 = alles identisch, 1 = Unterschiede
    return 0 if not differing and not only_project else 1


if __name__ == "__main__":
    sys.exit(main())
