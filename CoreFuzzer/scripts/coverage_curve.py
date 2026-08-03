#!/usr/bin/env python3
"""
Generate a coverage-vs-time curve for a specific source directory
(e.g. /corefuzzer_deps/open5gs/src/amf) from run_hourly.py lcov captures.

Each ./logs/app{i}_{j}.info is a CUMULATIVE tracefile (counters were only
zeroed once at the start), so extracting the target dir from each file and
running `lcov --summary` gives the coverage growth over time.

Usage (inside the corefuzzer container):
    python3 scripts/coverage_curve.py [logs_dir] [target_path]
Example:
    python3 scripts/coverage_curve.py ./logs /corefuzzer_deps/open5gs/src/amf

Outputs:
    ./logs/amf_coverage_by_time.csv        - per-interval cumulative coverage
    ./logs/amf_final.info                  - last tracefile filtered to target
    ./logs/amf_coverage/index.html         - final HTML report for the target dir
    (optional) ./logs/amf_coverage_curve.png  - plot if matplotlib is installed
"""
import os
import re
import sys
import glob
import subprocess

LOGS_DIR = sys.argv[1] if len(sys.argv) > 1 else "./logs"
TARGET = sys.argv[2] if len(sys.argv) > 2 else "/corefuzzer_deps/open5gs/src/amf"

# Same ignore-list as run_hourly.py so lcov never bails on noise.
# `inconsistent` (line hit but no branch evaluated) and `range` must be
# included, otherwise lcov refuses to read these tracefiles at all.
IGNORE = ("--ignore-errors branch,callback,child,corrupt,count,deprecated,empty,"
          "excessive,fork,format,gcov,graph,inconsistent,internal,mismatch,missing,negative,"
          "package,parallel,parent,range,source,unsupported,unused,usage,utility,version")


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError(f"cmd failed ({p.returncode}): {' '.join(cmd)}\n{p.stderr}")
    return p.stdout + p.stderr


def summary_of(info_file, out_info):
    # 1) keep only files under TARGET
    run(["lcov", "--extract", info_file, TARGET + "/*", "-o", out_info,
         "--rc", "lcov_branch_coverage=1"] + IGNORE.split())
    # 2) summarize the filtered tracefile
    return run(["lcov", "--summary", out_info,
                "--rc", "lcov_branch_coverage=1"] + IGNORE.split())


def parse_summary(text):
    def grab(key):
        m = re.search(rf"{key}\.+:\s*[\d.]+\%\s*\((\d+) of (\d+)", text)
        return (int(m.group(1)), int(m.group(2))) if m else (0, 0)

    lines = grab("lines")
    funcs = grab("functions")
    branches = grab("branches")
    pct = lambda h, t: round(h * 100.0 / t, 2) if t else 0.0
    return dict(lh=lines[0], lt=lines[1], lp=pct(*lines),
                fh=funcs[0], ft=funcs[1], fp=pct(*funcs),
                bh=branches[0], bt=branches[1], bp=pct(*branches))


def main():
    files = sorted(glob.glob(os.path.join(LOGS_DIR, "app*.info")))
    if not files:
        sys.exit(f"no app*.info found in {LOGS_DIR}")

    tmp = "/tmp/amf_tmp.info"
    csv_path = os.path.join(LOGS_DIR, "amf_coverage_by_time.csv")
    rows = []
    print(f"target : {TARGET}")
    print(f"found  : {len(files)} tracefiles")

    for f in files:
        base = os.path.basename(f)  # app{i}_{j}.info
        m = re.match(r"app(\d+)_(\d+)\.info", base)
        if not m:
            continue
        tmin = int(m.group(1)) * 60 + int(m.group(2)) * 10
        s = parse_summary(summary_of(f, tmp))
        rows.append((base, tmin, s))
        print(f"{base:14s} t={tmin:4d}min  "
              f"lines {s['lp']:6.2f}%  func {s['fp']:6.2f}%  branch {s['bp']:6.2f}%")

    with open(csv_path, "w") as fh:
        fh.write("interval,time_min,lines_hit,lines_total,line_pct,"
                 "func_hit,func_total,func_pct,branch_hit,branch_total,branch_pct\n")
        for base, tmin, s in rows:
            fh.write(f"{base},{tmin},{s['lh']},{s['lt']},{s['lp']},"
                     f"{s['fh']},{s['ft']},{s['fp']},{s['bh']},{s['bt']},{s['bp']}\n")
    print("wrote", csv_path)

    # Final HTML report restricted to the target dir only
    final_info = os.path.join(LOGS_DIR, "amf_final.info")
    run(["lcov", "--extract", files[-1], TARGET + "/*", "-o", final_info,
         "--rc", "lcov_branch_coverage=1"] + IGNORE.split())
    out_dir = os.path.join(LOGS_DIR, "amf_coverage")
    os.makedirs(out_dir, exist_ok=True)
    run(["genhtml", final_info, "-o", out_dir, "--branch-coverage",
         "--rc", "lcov_branch_coverage=1"] + IGNORE.split())
    print("wrote", os.path.join(out_dir, "index.html"))

    # Optional plot (skip gracefully if matplotlib is missing)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        xs = [r[1] for r in rows]
        plt.figure(figsize=(8, 5))
        plt.plot(xs, [r[2]["lp"] for r in rows], "-o", label="line")
        plt.plot(xs, [r[2]["fp"] for r in rows], "-s", label="function")
        plt.plot(xs, [r[2]["bp"] for r in rows], "-^", label="branch")
        plt.xlabel("time (min)")
        plt.ylabel("coverage (%)")
        plt.title(os.path.basename(TARGET.rstrip("/")))
        plt.legend()
        plt.grid(True)
        png = os.path.join(LOGS_DIR, "amf_coverage_curve.png")
        plt.savefig(png, dpi=150)
        print("wrote", png)
    except ImportError:
        print("matplotlib not installed - skipping plot (CSV still generated)")


if __name__ == "__main__":
    main()
