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
#include <limits>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
using std::vector;
using std::string;
using ofJson = nlohmann::json;
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
    void setMin(int) {}
    void setMax(int) {}
    void setWithoutEventNotifications(int v) { value = v; }
};
struct Dropdown {
    vector<string> names;
    void setDropdownOptions(const vector<string>& options) { names = options; }
};
struct Controller {
    vector<ofxLaser::Point> data, secondLaser;
    vector<float> appData;
    const vector<float>& getSimulatorData() const { return appData; }
    vector<string> names = {"Laser 1", "Laser 2"};
    uint64_t revision = 0;
    int reads = 0;
    uint64_t getLasersRevision() { return revision; }
    vector<string> getLaserNames() { return names; }
    vector<ofxLaser::Point> getLaserPoints(int index) {
        ++reads;
        assert(index >= 0 && index < static_cast<int>(names.size()));
        return index == 0 ? data : index == 1 ? secondLaser : vector<ofxLaser::Point>{};
    }
};
struct Simulator {
    bool disableUpdate = false, showWindow = false, windowVisible = false;
    int textureOut = 0, debugVertexCount = 0, debugShapeCount = 0, debugDotCount = 0;
    static constexpr int STRIDE = 5;
    Output output;
    std::shared_ptr<Controller> controller = std::make_shared<Controller>();
    Selection laserIndex;
    std::shared_ptr<Dropdown> laserDropdown = std::make_shared<Dropdown>();
    int availableLaserCount = -1, viewMode = 0;
    enum DataSource { DataIlda = 0, DataApp = 1 };
    int dataSource = DataIlda;
    uint64_t knownLasersRevision = std::numeric_limits<uint64_t>::max();
    vector<float> selectedLaserData, renderedData;
    int rebuilds = 0, renders = 0;
    Output& getOceanodeParameter(int) { return output; }
    REFRESH_LASERS
    void rebuildGeometry(const vector<float>& data) { ++rebuilds; renderedData = data; }
    void renderToFbo() { ++renders; }
    REBUILD_DATA
    APPEND_DATA
    SAVE_PRESET
    RECALL_PRESET
    UPDATE
};
int main() {
    Simulator s;
    ofEventArgs args;
    s.update(args);
    assert(s.renders == 0); // frame zero must not imply view demand
    assert(s.laserIndex.get() == 0); // default selection is still the first projector
    assert(s.laserDropdown->names == vector<string>({"Laser 1", "Laser 2", "All Lasers"}));
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
    // ofxLaser writes a dot as repeated identical points (dotMaxPoints), the
    // last one possibly dimmer: it must stay a one-point run with the brightest
    // colour, or it renders as zero-length segments, i.e. nothing. Repeats
    // inside a line (corner dwell) merge without splitting the run.
    s.controller->secondLaser = {{400,400,255,0,0}, {400,400,255,0,0}, {400,400,128,64,0},
                                {0,0,0,0,0},
                                {0,0,0,255,0}, {80,0,0,255,0}, {80,0,0,0,255}, {160,0,0,255,0}};
    s.update(args);
    assert(s.debugVertexCount == 4 && s.debugShapeCount == 2 && s.debugDotCount == 1);
    assert(s.renderedData.size() == 22 && s.renderedData[5] == -1.0f);
    assert(s.renderedData[0] == 0.5f && s.renderedData[2] == 1.0f && s.renderedData[3] == 64.0f / 255.0f);
    assert(s.renderedData[11] == 0.1f && s.renderedData[14] == 1.0f && s.renderedData[15] == 1.0f);
    s.controller->secondLaser = {{100,100,0,0,0}};
    s.update(args);
    assert(s.renders == 8 && s.renderedData.empty());
    // Both final projector outputs share the projection plane. Each projector
    // ends with a separator even if its final point is lit.
    s.controller->data = {{80,160,255,0,0}};
    s.controller->secondLaser = {{640,720,0,255,0}};
    s.laserIndex.value = 2;
    s.update(args);
    assert(s.controller->reads == 10 && s.debugShapeCount == 2 && s.debugDotCount == 2);
    assert(s.renderedData.size() == 12 && s.renderedData[5] == -1 && s.renderedData[11] == -1);
    assert(s.renderedData[0] == 0.1f && s.renderedData[6] == 0.8f);
    ofJson allPreset;
    s.presetSave(allPreset);
    assert(allPreset["SimulateAllLasers"].get<bool>());
    // The appended All entry follows additions/deletions instead of turning
    // into a particular projector when the list changes.
    s.controller->names.push_back("Laser 3");
    ++s.controller->revision;
    s.update(args);
    assert(s.laserIndex.get() == 3 && s.controller->reads == 13);
    assert(s.debugShapeCount == 2); // empty third projector introduces no phantom shape
    // Recall All saved with two projectors while three are configured.
    s.laserIndex.value = 2;
    s.presetRecallAfterSettingParameters(allPreset);
    assert(s.laserIndex.get() == 3);
    // Legacy presets keep individual indices, and an invalid old index must
    // clamp to a projector rather than inadvertently selecting All.
    ofJson legacy = ofJson::object();
    s.laserIndex.value = 1;
    s.presetRecallAfterSettingParameters(legacy);
    assert(s.laserIndex.get() == 1);
    s.laserIndex.value = 3;
    s.presetRecallAfterSettingParameters(legacy);
    assert(s.laserIndex.get() == 2);
    // Deleting the selected last projector also clamps to a remaining one.
    s.controller->names.pop_back();
    ++s.controller->revision;
    s.refreshLasers();
    assert(s.laserIndex.get() == 1);
    s.laserIndex.value = 2; // All with two projectors
    s.controller->names.resize(1);
    ++s.controller->revision;
    s.update(args);
    assert(s.laserIndex.get() == 1 && s.debugShapeCount == 1);
    s.controller->names.clear();
    ++s.controller->revision;
    const int reads = s.controller->reads;
    s.update(args);
    assert(s.laserIndex.get() == 0 && s.renderedData.empty() && s.controller->reads == reads);
    // App shows the shapes ILDA Shape nodes publish, before ofxLaser, without
    // reading any laser; ILDA, the default, shows the selected laser's points.
    ofJson ildaPreset;
    s.presetSave(ildaPreset);
    assert(ildaPreset["DataSource"] == "ILDA");
    s.dataSource = Simulator::DataApp;
    s.controller->appData = {0.5f,0.5f,1,0,0,-1, 0.1f,0.2f,0,1,0, 0.3f,0.4f,0,1,0,-1};
    s.update(args);
    assert(s.renderedData == s.controller->appData && s.controller->reads == reads);
    assert(s.debugVertexCount == 3 && s.debugShapeCount == 2 && s.debugDotCount == 1);
    ofJson appPreset;
    s.presetSave(appPreset);
    assert(appPreset["DataSource"] == "App");
    s.presetRecallAfterSettingParameters(ildaPreset);
    assert(s.dataSource == Simulator::DataIlda);
    s.presetRecallAfterSettingParameters(appPreset);
    assert(s.dataSource == Simulator::DataApp);
    s.presetRecallAfterSettingParameters(legacy); // presets from before Data open on ILDA
    assert(s.dataSource == Simulator::DataIlda);
    frame = 0;
    assert(!s.output.wasUsedByView()); // no underflow if frame counter resets
}
'''
    code = code.replace("MARK_VIEW", method(parameter, "void markUsedByView("))
    code = code.replace("USED_VIEW", method(parameter, "bool wasUsedByView("))
    code = code.replace("UPDATE", method(simulator, "void update(ofEventArgs&"))
    code = code.replace("REBUILD_DATA", method(simulator, "void rebuildSelectedLaserData("))
    code = code.replace("APPEND_DATA", method(simulator, "void appendLaserData("))
    code = code.replace("REFRESH_LASERS", method(simulator, "void refreshLasers("))
    code = code.replace("SAVE_PRESET", method(simulator, "void presetSave("))
    code = code.replace("RECALL_PRESET", method(simulator, "void presetRecallAfterSettingParameters("))
    with tempfile.TemporaryDirectory(prefix="laser-view-") as directory:
        source, binary = Path(directory) / "test.cpp", Path(directory) / "test"
        source.write_text(code)
        subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-I", str(ROOT.parent.parent / "libs/json/include"),
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print("Simulator regression passed: view demand, single/all projectors, boundaries, blanking, dots and repeated points, ILDA/App data, preset recall, list changes, empty data.")


if __name__ == "__main__":
    main()
