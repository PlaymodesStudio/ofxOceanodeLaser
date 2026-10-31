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
struct Controller {
    vector<float> data;
    int reads = 0;
    const vector<float>& getSimulatorData() { ++reads; return data; }
};
struct Simulator {
    bool disableUpdate = false, showWindow = false, windowVisible = false;
    int textureOut = 0, debugVertexCount = 0, debugShapeCount = 0, debugDotCount = 0;
    static constexpr int STRIDE = 5;
    Output output;
    std::shared_ptr<Controller> controller = std::make_shared<Controller>();
    int rebuilds = 0, renders = 0;
    Output& getOceanodeParameter(int) { return output; }
    void rebuildGeometry(const vector<float>&) { ++rebuilds; }
    void renderToFbo() { ++renders; }
    UPDATE
};
int main() {
    Simulator s;
    ofEventArgs args;
    s.update(args);
    assert(s.renders == 0); // frame zero must not imply view demand
    s.controller->data = {0,0,1,1,1,-1};
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
    frame = 0;
    assert(!s.output.wasUsedByView()); // no underflow if frame counter resets
}
'''
    code = code.replace("MARK_VIEW", method(parameter, "void markUsedByView("))
    code = code.replace("USED_VIEW", method(parameter, "bool wasUsedByView("))
    code = code.replace("UPDATE", method(simulator, "void update(ofEventArgs&"))
    with tempfile.TemporaryDirectory(prefix="laser-view-") as directory:
        source, binary = Path(directory) / "test.cpp", Path(directory) / "test"
        source.write_text(code)
        subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print("Simulator view regression passed: GUI demand, expiry, cables, window visibility, preset guard, empty data.")


if __name__ == "__main__":
    main()
