#!/usr/bin/env python3
"""Exercise production simulator update and view demand without a GL context."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def method(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end].replace(" override", "")


def main():
    parameter = (ROOT.parent / "ofxOceanode/src/ofxOceanodeParameter.h").read_text()
    simulator = (ROOT / "src/laserSimulator.h").read_text()
    code = r'''
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
using std::vector;
uint64_t frame = 0;
uint64_t ofGetFrameNum() { return frame; }
struct Output {
    uint64_t lastViewFrame = 0;
    bool hasViewFrame = false;
    bool connected = false;
    bool hasOutConnections() { return connected; }
    MARK_VIEW
    USED_VIEW
};
struct ofEventArgs {};
namespace ofxLaser {
struct Point { float x, y, r, g, b; };
}
float ofClamp(float value, float low, float high) { return std::clamp(value, low, high); }
struct Selection {
    int value = 0;
    int get() { return value; }
};
struct Controller {
    vector<ofxLaser::Point> data, secondLaser;
    int reads = 0;
    vector<ofxLaser::Point> getLaserPoints(int index) {
        ++reads;
        return index == 0 ? data : secondLaser;
    }
};
struct Simulator {
    bool disableUpdate = false, showWindow = false, windowVisible = false;
    int textureOut = 0, debugVertexCount = 0, debugShapeCount = 0, debugDotCount = 0;
    static constexpr int STRIDE = 5;
    Output output;
    std::shared_ptr<Controller> controller = std::make_shared<Controller>();
    Selection laserIndex;
    vector<float> selectedLaserData, renderedData;
    int rebuilds = 0, renders = 0;
    Output& getOceanodeParameter(int) { return output; }
    void refreshLasers() {}
    void rebuildGeometry(const vector<float>& data) { ++rebuilds; renderedData = data; }
    void renderToFbo() { ++renders; }
    REBUILD_DATA
    UPDATE
};
int main() {
    Simulator s;
    ofEventArgs args;
    s.update(args);
    assert(s.renders == 0); // frame zero must not imply view demand
    s.controller->data = {{0,0,255,255,255}};
    s.output.markUsedByView(); // visible Custom GUI, no cable, Show off
    ++frame;
    s.update(args);
    assert(s.renders == 1 && s.debugDotCount == 1);
    s.output.markUsedByView();
    ++frame;
    s.update(args);
    assert(s.renders == 2); // continuous drawing keeps it live
    frame += 2;
    s.update(args);
    assert(s.renders == 2); // removed/hidden widget releases demand
    s.output.connected = true;
    s.update(args);
    assert(s.renders == 3); // cable works with Show off
    s.disableUpdate = true;
    s.update(args);
    assert(s.renders == 3); // preset-load guard still wins
    s.disableUpdate = false;
    s.output.connected = false;
    s.showWindow = true;
    s.update(args);
    assert(s.renders == 3); // hidden/collapsed simulator pauses
    s.windowVisible = true;
    s.update(args);
    assert(s.renders == 4);
    s.showWindow = false;
    s.update(args);
    assert(s.renders == 4); // stale windowVisible cannot override Show
    s.output.markUsedByView();
    s.controller->data.clear();
    s.update(args);
    assert(s.renders == 5 && s.debugDotCount == 0); // clear stale shapes
    assert(s.controller->reads == 5 && s.rebuilds == 5);
    // Select only the second projector, normalize coordinates/colours, and
    // split lit runs at blanking points rather than drawing travel moves.
    s.controller->secondLaser = {{400,800,255,128,0}, {800,400,0,255,255},
                                {0,0,0,0,0}, {200,200,0,0,255}};
    s.laserIndex.value = 1;
    s.update(args);
    assert(s.renders == 6 && s.controller->reads == 6);
    assert(s.debugVertexCount == 3 && s.debugShapeCount == 2 && s.debugDotCount == 1);
    assert(s.renderedData.size() == 17);
    assert(s.renderedData[0] == 0.5f && s.renderedData[1] == 1.0f);
    assert(s.renderedData[2] == 1.0f && s.renderedData[3] == 128.0f / 255.0f);
    assert(s.renderedData[10] == -1.0f && s.renderedData.back() == -1.0f);
    s.controller->secondLaser = {{100,100,0,0,0}};
    s.update(args);
    assert(s.renders == 7 && s.renderedData.empty());
    frame = 0;
    assert(!s.output.wasUsedByView()); // no underflow if frame counter resets
}
'''
    code = code.replace("MARK_VIEW", method(parameter, "void markUsedByView("))
    code = code.replace("USED_VIEW", method(parameter, "bool wasUsedByView("))
    code = code.replace("UPDATE", method(simulator, "void update(ofEventArgs&"))
    code = code.replace("REBUILD_DATA", method(simulator, "void rebuildSelectedLaserData("))
    with tempfile.TemporaryDirectory(prefix="laser-view-") as directory:
        source, binary = Path(directory) / "test.cpp", Path(directory) / "test"
        source.write_text(code)
        subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print("Simulator regression passed: view demand, preset guard, projector selection, blanking, normalization, empty data.")


if __name__ == "__main__":
    main()
