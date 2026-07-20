#!/usr/bin/env python3
"""Turn the language-neutral shared fixture (fixtures/tempo_curve_cases.json) into a C++
header the native fixture runner embeds, so the native side needs no runtime JSON parser.
The JSON stays the single committed source of truth; this header is a build artifact.

Usage: json_to_header.py <fixture.json> <out_header.h>
"""
import json
import os
import sys


def d(x):
    # emit a double literal that round-trips the JSON value exactly
    return repr(float(x))


def main():
    src, out = sys.argv[1], sys.argv[2]
    with open(src) as f:
        fx = json.load(f)

    lines = [
        "// GENERATED from fixtures/tempo_curve_cases.json by tools/json_to_header.py — do not edit.",
        "#ifndef FIXTURE_GENERATED_H",
        "#define FIXTURE_GENERATED_H",
        "#include <vector>",
        "namespace fixture {",
        "struct Pt { double pos, bpm, bend; };",
        "struct Q { double in, expected; };",
        "struct Case { const char* name; double tolerance; std::vector<Pt> points;"
        " std::vector<Q> pos2time; std::vector<Q> time2pos; };",
        f"inline int resolution() {{ return {int(fx['resolution'])}; }}",
        "inline const std::vector<Case>& cases() {",
        "  static const std::vector<Case> c = {",
    ]

    for case in fx["cases"]:
        pts = ", ".join(
            f"{{{d(p['pos'])}, {d(p['bpm'])}, {d(p['bend'])}}}" for p in case["points"]
        )
        p2t = ", ".join(
            f"{{{d(q['pos'])}, {d(q['time'])}}}" for q in case.get("pos2time", [])
        )
        t2p = ", ".join(
            f"{{{d(q['time'])}, {d(q['pos'])}}}" for q in case.get("time2pos", [])
        )
        name = json.dumps(case["name"])  # C string literal escaping
        lines.append(
            f"    {{{name}, {d(case['tolerance'])}, {{{pts}}}, {{{p2t}}}, {{{t2p}}}}},"
        )

    lines += ["  };", "  return c;", "}", "}  // namespace fixture", "#endif  // FIXTURE_GENERATED_H", ""]

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
