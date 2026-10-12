#!/usr/bin/env python3
"""Exercise the production ILDA Shape listener with masks and zone routing, without a DAC."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OF_ROOT = Path(os.environ.get("OCEANODE_TEST_OF_ROOT", ROOT.parent.parent))


def body(source, signature):
    opening = source.index("{", source.index(signature))
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[opening:end]


def main():
    shape = (ROOT / "src/ildaShape.h").read_text()
    code = r'''
#include "ildaMaskField.h"
#include <cassert>
#include <memory>
#include <numeric>
#include <string>
using std::vector;
using std::string;
struct ofFloatColor {
    float r=0, g=0, b=0, a=1;
    ofFloatColor() = default;
    ofFloatColor(float R, float G, float B, float A=1):r(R),g(G),b(B),a(A){}
    ofFloatColor getLerped(const ofFloatColor& c, float t) const {
        return {r+(c.r-r)*t,g+(c.g-g)*t,b+(c.b-b)*t,a+(c.a-a)*t};
    }
    static const ofFloatColor black;
};
const ofFloatColor ofFloatColor::black{};
struct ofColor {
    int r=0,g=0,b=0;
    ofColor() = default;
    explicit ofColor(const ofFloatColor& c):r(c.r*255),g(c.g*255),b(c.b*255){}
};
struct ofPolyline {
    vector<glm::vec3> vertices;
    bool closed = false;
    explicit ofPolyline(const vector<glm::vec3>& v):vertices(v){}
    vector<glm::vec3>& getVertices() { return vertices; }
    const vector<glm::vec3>& getVertices() const { return vertices; }
    void setClosed(bool b) { closed=b; }
    bool isClosed() const { return closed; }
    size_t size() const { return vertices.size(); }
};
struct ofxFatLine : ofPolyline {
    vector<ofFloatColor> colors;
    ofxFatLine(vector<glm::vec3> v, vector<ofFloatColor> c):ofPolyline(v),colors(c){}
    const vector<ofFloatColor>& getColors() const { return colors; }
};
struct ofRectangle {
    float x=7,y=8,width=9,height=10;
    void set(float X,float Y,float W,float H) { x=X;y=Y;width=W;height=H; }
};
struct Manager {
    bool useClipRectangle = false, drawnWithClip = false, wasDot = false, closed = false;
    ofRectangle clipRectangle, drawnClip;
    vector<glm::vec3> vertices;
    vector<ofColor> colors;
    int draws = 0;
    void drawPoly(ofPolyline& p, const vector<ofColor>& c, const string&) {
        ++draws; vertices=p.vertices; colors=c; closed=p.closed;
        drawnWithClip=useClipRectangle; drawnClip=clipRectangle; wasDot=false;
    }
    void drawDot(glm::vec3 p, ofColor c, int, const string& profile) {
        ofPolyline poly({p}); drawPoly(poly,{c},profile); wasDot=true;
    }
};
struct ildaController {
    static constexpr float ZoneSize=800;
    bool frozen=false, masked=false;
    int zone=1;
    Manager manager;
    bool isFrozen() { return frozen; }
    bool hasActiveMask() { return masked; }
    float sampleMask(float u,float v) {
        assert(u>=0 && u<=1 && v>=0 && v<=1); // sampling must precede zone offset
        return u>0.3f && u<0.4f ? 0.0f : 1.0f;
    }
    glm::vec2 getZoneOffset(const string&) { return {zone*ZoneSize,0}; }
    Manager& getManager() { return manager; }
    void publishLaserShape(const vector<glm::vec3>& v,const vector<ofColor>& c) { assert(v.size()==c.size()); }
};
struct Shape {
    std::shared_ptr<ildaController> controller=std::make_shared<ildaController>();
    bool disable=false, sendBlackShapes=true;
    float blackThreshold=0;
    int renderProfile=0;
    string selectedZoneId="zone-2";
    vector<string> options={"Fast"};
    vector<ofxFatLine> storedInput;
    vector<glm::vec3> maskedVertices;
    vector<ildaMasking::MaskedVertex> maskedSamples;
    static constexpr float maskSpacing=1.0f/200, maskTolerance=0.02f;
    void refreshZones() {}
    void applyMask(const ofxFatLine &fat, const vector<ofFloatColor> &sourceColors,
                   vector<glm::vec3> &vertices, vector<ofColor> &colors) MASK_BODY
    void input(vector<ofxFatLine>& vf) LISTENER_BODY
};
bool near(float a,float b) { return std::abs(a-b)<0.001f; }
int main() {
    Shape s;
    auto& controller=*s.controller;
    auto& manager=controller.manager;
    vector<ofxFatLine> line={ofxFatLine({{0,400,0},{800,400,0}},{{1,1,1},{1,1,1}})};
    line[0].setClosed(true);
    s.input(line);
    assert(manager.draws==1 && manager.vertices.size()==2 && manager.closed);
    assert(near(manager.vertices[0].x,800) && near(manager.vertices[1].x,1600));
    assert(near(line[0].getVertices()[0].x,0)); // routing does not mutate upstream input
    assert(manager.drawnWithClip && near(manager.drawnClip.x,800.01f));
    assert(near(manager.drawnClip.width,799.98f));
    assert(!manager.useClipRectangle && near(manager.clipRectangle.x,7));
    controller.masked=true;
    s.input(line);
    assert(manager.vertices.size()>2 && manager.vertices.size()==manager.colors.size());
    bool foundBlack=false;
    for(size_t i=0;i<manager.vertices.size();++i) {
        assert(manager.vertices[i].x>=800 && manager.vertices[i].x<=1600);
        if(manager.vertices[i].x>1040 && manager.vertices[i].x<1120 && manager.colors[i].r==0) foundBlack=true;
    }
    assert(foundBlack && manager.closed);
    controller.frozen=true;
    vector<ofxFatLine> dot={ofxFatLine({{100,100,0}},{{0.5f,0.5f,0.5f,0.5f}})};
    s.input(dot);
    assert(!manager.wasDot && manager.vertices.size()>2); // freeze retains last line
    controller.frozen=false; controller.masked=false; controller.zone=0;
    s.input(dot);
    assert(manager.wasDot && near(manager.vertices[0].x,100));
    assert(manager.colors[0].r==127); // preserve premultiplied RGB; no second alpha multiplication
    const int draws=manager.draws;
    s.disable=true; s.input(line); assert(manager.draws==draws);
    s.disable=false; s.sendBlackShapes=false; s.blackThreshold=0.9f;
    s.input(dot); assert(manager.draws==draws);
}
'''
    code = code.replace("MASK_BODY", body(shape, "void applyMask("))
    code = code.replace("LISTENER_BODY", body(shape, "listener = vInput.newListener("))
    with tempfile.TemporaryDirectory(prefix="multilaser-routing-") as tmp:
        source, binary = Path(tmp) / "test.cpp", Path(tmp) / "test"
        source.write_text(code)
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-I", str(ROOT / "src"), "-I", str(OF_ROOT / "libs/glm/include"),
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print("Multilaser routing regression passed: masks, zones, clipping state, freeze, preset guard, threshold, alpha.")


if __name__ == "__main__":
    main()
