#!/usr/bin/env python3
"""Build the report with embedded macOS Songti/Heiti/Menlo fonts.

Requires Pandoc and XeLaTeX. Run from any directory:
    python3 report/build_report.py
Optional: --input LOCAL.md --output LOCAL.pdf
Keep personal input/output files in the ignored workspaces/ directory.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    report = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=report / "report.md")
    parser.add_argument("--output", type=Path, default=report / "report.pdf")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="cachelab-report-") as name:
        build = Path(name)
        shutil.copytree(report / "img", build / "img")
        tex = build / "report.tex"
        subprocess.run([
            "pandoc", str(args.input.resolve()), "-s", "-o", str(tex),
            "-H", str(report / "format.tex"), "-V", "geometry:margin=20mm",
            "-V", "papersize=a4", "-V", "fontsize=11pt", "-V", "colorlinks=true",
            "-V", "documentclass=article", "--syntax-highlighting=tango",
        ], check=True)
        for _ in range(2):
            subprocess.run(["xelatex", "-interaction=nonstopmode", "-halt-on-error",
                            "report.tex"], cwd=build, check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / "report.pdf", args.output)
        print(args.output.resolve())


if __name__ == "__main__":
    main()
