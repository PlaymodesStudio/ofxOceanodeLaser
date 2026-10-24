#!/usr/bin/env python3
"""Checks the ILDA Mask math in src/ildaMaskField.h.

Builds a small C++ harness against glm only (no openFrameworks), with address and
undefined-behaviour sanitizers, and runs it. Usage: python3 tests/ilda_mask_regression.py
OCEANODE_TEST_OF_ROOT overrides the openFrameworks root, CXX the compiler.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OF_ROOT = Path(os.environ.get("OCEANODE_TEST_OF_ROOT", ROOT.parent.parent))
GLM = OF_ROOT / "libs/glm/include"

HARNESS = r'''
#include "ildaMaskField.h"
#include <cstdio>
#include <cstdlib>

using namespace ildaMasking;

static int failures = 0;
#define CHECK(cond) do { if(!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while(0)
static bool near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }

int main() {
    // Ellipse: inside passes, outside is black, feather fades inwards from the edge.
    ShapeParams e; e.x = 0.5f; e.y = 0.5f; e.w = 0.5f; e.h = 0.5f; e.feather = 0.1f;
    CHECK(near(evaluateShape(Shape::Ellipse, e, 0.5f, 0.5f), 1.0f));
    CHECK(near(evaluateShape(Shape::Ellipse, e, 0.76f, 0.5f), 0.0f));
    CHECK(near(evaluateShape(Shape::Ellipse, e, 0.75f, 0.5f), 0.0f));
    CHECK(near(evaluateShape(Shape::Ellipse, e, 0.70f, 0.5f), 0.5f));
    CHECK(near(evaluateShape(Shape::Ellipse, e, 0.64f, 0.5f), 1.0f));
    e.feather = 0.0f;
    CHECK(evaluateShape(Shape::Ellipse, e, 0.74f, 0.5f) == 1.0f);
    CHECK(evaluateShape(Shape::Ellipse, e, 0.76f, 0.5f) == 0.0f);

    // Rect and rotation: a 0.6 x 0.2 rect turned 90 degrees covers (0.5, 0.75) but not (0.75, 0.5).
    ShapeParams r; r.w = 0.6f; r.h = 0.2f; r.feather = 0.0f;
    CHECK(evaluateShape(Shape::Rect, r, 0.75f, 0.5f) == 1.0f);
    CHECK(evaluateShape(Shape::Rect, r, 0.5f, 0.75f) == 0.0f);
    r.angle = 90.0f;
    CHECK(evaluateShape(Shape::Rect, r, 0.75f, 0.5f) == 0.0f);
    CHECK(evaluateShape(Shape::Rect, r, 0.5f, 0.75f) == 1.0f);

    // Linear: passes on the left of x at angle 0, on the right at 180.
    ShapeParams l; l.x = 0.5f; l.feather = 0.2f;
    CHECK(near(evaluateShape(Shape::Linear, l, 0.1f, 0.5f), 1.0f));
    CHECK(near(evaluateShape(Shape::Linear, l, 0.6f, 0.5f), 0.0f));
    CHECK(near(evaluateShape(Shape::Linear, l, 0.4f, 0.9f), 0.5f));
    l.angle = 180.0f;
    CHECK(near(evaluateShape(Shape::Linear, l, 0.1f, 0.5f), 0.0f));

    // Field: union of shapes, invert, amount, identity detection.
    Field f;
    CHECK(f.isIdentity());
    f.shape = Shape::Ellipse;
    ShapeParams left; left.x = 0.25f; left.w = 0.2f; left.h = 0.2f; left.feather = 0.0f;
    ShapeParams right = left; right.x = 0.75f;
    f.shapes = {left, right};
    CHECK(!f.isIdentity());
    CHECK(f.sample(0.25f, 0.5f) == 1.0f);
    CHECK(f.sample(0.75f, 0.5f) == 1.0f);
    CHECK(f.sample(0.5f, 0.5f) == 0.0f);
    f.invert = true;
    CHECK(f.sample(0.5f, 0.5f) == 1.0f);
    CHECK(f.sample(0.25f, 0.5f) == 0.0f);
    f.amount = 0.25f;
    CHECK(near(f.sample(0.25f, 0.5f), 0.75f));
    f.amount = 0.0f;
    CHECK(f.isIdentity());
    Field black; black.invert = true;
    CHECK(!black.isIdentity());
    CHECK(black.sample(0.3f, 0.3f) == 0.0f);

    // Grid: bilinear between texel centres, clamped at the edges, row 0 is the top.
    Grid g; g.width = 2; g.height = 2; g.values = {0.0f, 1.0f, 0.0f, 1.0f};
    CHECK(near(g.sample(0.25f, 0.5f), 0.0f));
    CHECK(near(g.sample(0.75f, 0.5f), 1.0f));
    CHECK(near(g.sample(0.5f, 0.5f), 0.5f));
    CHECK(near(g.sample(-1.0f, 2.0f), 0.0f));
    Grid top; top.width = 1; top.height = 2; top.values = {1.0f, 0.0f};
    CHECK(near(top.sample(0.5f, 0.0f), 1.0f));
    CHECK(near(top.sample(0.5f, 1.0f), 0.0f));
    Field textured; textured.useGrid = true; textured.grid = g;
    CHECK(!textured.isIdentity());
    CHECK(near(textured.sample(0.5f, 0.5f), 0.5f));
    Grid broken; broken.width = 4; broken.height = 4;
    CHECK(broken.sample(0.5f, 0.5f) == 1.0f);

    const float canvas = 800.0f;
    const float spacing = 1.0f / 400.0f;
    const float tolerance = 0.01f;
    std::vector<MaskedVertex> out;

    // A flat mask adds no vertex.
    auto flat = [](float, float) { return 0.4f; };
    std::vector<glm::vec3> line = {{0, 400, 0}, {800, 400, 0}};
    maskPolyline(line, false, canvas, flat, spacing, tolerance, out);
    CHECK(out.size() == 2);
    CHECK(near(out[0].mask, 0.4f) && near(out[1].mask, 0.4f));
    CHECK(out[1].sourceIndex == 1.0f);

    // Crossing a feathered ellipse: the linear interpolation between output vertices
    // stays within tolerance of the true mask everywhere along the line.
    Field ellipse; ellipse.shape = Shape::Ellipse;
    ShapeParams es; es.feather = 0.1f; ellipse.shapes = {es};
    auto sampleEllipse = [&](float u, float v) { return ellipse.sample(u, v); };
    maskPolyline(line, false, canvas, sampleEllipse, spacing, tolerance, out);
    CHECK(out.size() > 2);
    CHECK(out.size() < 200);
    float worst = 0.0f;
    for (size_t i = 0; i + 1 < out.size(); i++) {
        CHECK(out[i].sourceIndex <= out[i + 1].sourceIndex);
        for (int k = 0; k <= 20; k++) {
            const float t = k / 20.0f;
            const glm::vec3 p = glm::mix(out[i].position, out[i + 1].position, t);
            const float interpolated = glm::mix(out[i].mask, out[i + 1].mask, t);
            worst = std::max(worst, std::abs(interpolated - ellipse.sample(p.x / canvas, p.y / canvas)));
        }
    }
    CHECK(worst < 0.03f);

    // A thin hard band in the middle of a segment, missed by its end points and midpoint, is still found.
    auto band = [](float u, float) { return (u > 0.3f && u < 0.31f) ? 0.0f : 1.0f; };
    maskPolyline(line, false, canvas, band, spacing, tolerance, out);
    bool foundDark = false;
    for (auto &v : out) foundDark |= v.mask == 0.0f;
    CHECK(foundDark);

    // Closed polylines also resample the closing segment; its indices run from n-1 towards n.
    std::vector<glm::vec3> triangle = {{100, 400, 0}, {700, 400, 0}, {400, 100, 0}};
    auto halfPlane = [](float u, float) { return u < 0.3f ? 1.0f : 0.0f; };
    maskPolyline(triangle, true, canvas, halfPlane, spacing, tolerance, out);
    bool closingSegmentSplit = false;
    for (auto &v : out) closingSegmentSplit |= v.sourceIndex > 2.0f && v.sourceIndex < 3.0f;
    CHECK(closingSegmentSplit);
    CHECK(out.back().sourceIndex < 3.0f);
    maskPolyline(triangle, false, canvas, halfPlane, spacing, tolerance, out);
    for (auto &v : out) CHECK(v.sourceIndex <= 2.0f);

    // Dots and empty input.
    std::vector<glm::vec3> dot = {{400, 400, 0}};
    maskPolyline(dot, false, canvas, sampleEllipse, spacing, tolerance, out);
    CHECK(out.size() == 1 && near(out[0].mask, 1.0f));
    maskPolyline(std::vector<glm::vec3>{}, true, canvas, sampleEllipse, spacing, tolerance, out);
    CHECK(out.empty());

    if (failures == 0) std::printf("ilda_mask_regression: OK\n");
    return failures == 0 ? 0 : 1;
}
'''


def main():
    if not GLM.is_dir():
        print(f"glm not found at {GLM}; set OCEANODE_TEST_OF_ROOT")
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "harness.cpp"
        binary = Path(tmp) / "harness"
        source.write_text(HARNESS)
        compile_cmd = [os.environ.get("CXX", "clang++"), "-std=c++17", "-g",
                       "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                       "-I", str(ROOT / "src"), "-I", str(GLM), str(source), "-o", str(binary)]
        subprocess.run(compile_cmd, check=True)
        return subprocess.run([str(binary)]).returncode


if __name__ == "__main__":
    sys.exit(main())
