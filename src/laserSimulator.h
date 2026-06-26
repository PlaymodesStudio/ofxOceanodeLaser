//
//  laserSimulator.h
//  ofxOceanodeLaser
//
//  Renders a real-time 3D simulation of all ILDA laser shapes currently
//  being sent to the laser, inside an ImGui window with a dynamic FBO.
//  The FBO starts at 1024x1024 and is reallocated to the smallest of
//  the ImGui window's content width/height whenever the window is resized.
//
//  Capture mechanism: ildaShape calls controller->publishLaserShape() for
//  every fatline that passes the black-threshold test, pushing a flat
//  [x, y, r, g, b, ..., -1] buffer (x/y normalised by 800, colours 0..1).
//  This node reads that buffer each frame and rebuilds beam geometry.
//
//  World convention:
//    - Floor  = XY plane (Z = 0)
//    - Beam fires along +Z toward the laser mesh plane at Z = LaserDistance
//    - Camera orbit: Azimuth around Z, Elevation from XY plane
//
//  See plans/laser-simulator-node-plan.md for full design rationale.
//

#ifndef laserSimulator_h
#define laserSimulator_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"
#include "ofMain.h"

class laserSimulator : public ofxOceanodeNodeModel {
public:
    laserSimulator(shared_ptr<ildaController> iController)
        : controller(iController)
        , ofxOceanodeNodeModel("Laser Simulator")
    {}

    // ------------------------------------------------------------------
    // setup() — all parameter wiring lives here (never in the constructor)
    // ------------------------------------------------------------------
    void setup() override {

        // --- Projection ---
		addParameter(nearWidth.set("Near Width", 0.0f, 0.0f, 2.0f));
		addParameter(sphericalProjection.set("Spherical Proj", true));
        addParameter(fovX.set("FOV X",     60.0f,  1.0f, 120.0f));
        addParameter(fovY.set("FOV Y",     60.0f,  1.0f, 120.0f));
        addParameter(offsetX.set("Offset X",  0.0f, -1.0f, 1.0f));
        addParameter(offsetY.set("Offset Y",  0.0f, -1.0f, 1.0f));
        addParameter(laserDistance.set("Laser Dist", 5.0f, 0.1f, 50.0f));

        // --- Scene geometry ---
        addParameter(floorThickness.set("Floor Thick",    0.05f, 0.0f, 1.0f));
        addParameter(pointCylinderSize.set("Dot Cyl Size",  0.05f, 0.0f, 1.0f));
        addParameter(pointProjectionSize.set("Dot Proj Size", 0.1f, 0.0f, 2.0f));
  addParameter(pointCylinderRes.set("Dot Cyl Res",  8, 3, 32));
        addParameter(pointProjectionRes.set("Dot Proj Res", 8, 3, 32));
        addParameter(gridCurveRes.set("Grid Curve Res", 32, 2, 128));

        // --- Camera ---
        // Orbit/distance driven by mouse; Near/Far exposed as adjustable params.
        addParameter(camNear.set("Cam Near", 0.01f,  0.001f,  10.0f));
        addParameter(camFar.set("Cam Far",  30.0f, 10.0f,  100.0f));

        easyCam.disableMouseInput();
        easyCam.setNearClip(camNear);
        easyCam.setFarClip(camFar);
        // After swapYZ with negation: laser head is at Y = -laserDistance.
        // Orbit center is halfway between floor (Y=0) and laser head (Y=-LD).
        glm::vec3 orbitCenter(0.0f, -(float)laserDistance.get() * 0.5f, 0.0f);
        easyCam.orbit(camAzimuth, camElevation, camOrbDist, orbitCenter);
		
        // Update clip planes when params change
        parameterListeners.push(camNear.newListener([this](float &v){
            easyCam.setNearClip(v);
        }));
        parameterListeners.push(camFar.newListener([this](float &v){
            easyCam.setFarClip(v);
        }));

        // --- Gizmos ---
        addParameter(drawFloor.set("Grid",    true));
        addParameter(drawAxis.set("Axis",     true));
        addParameter(drawFrustum.set("Frustum", false));

        // --- Window ---
        addParameter(showWindow.set("Show", true));

        // --- Output ---
        addParameter(textureOut.set("Texture", nullptr, nullptr, nullptr));

        // --- Rendering ---
        //addParameter(renderLikeUnity.set("RenderLikeUnity", true));
        addParameter(brightness.set("Brightness", 0.75f, 0.0f, 1.0f));

        // Allocate FBO at initial size; will be reallocated on window resize
        reallocFbo(currentFboSize);
    }

    // ------------------------------------------------------------------
    // Preset safety stubs.
    // laserSimulator holds no ofxFatLine listeners of its own (it reads
    // the shared buffer), so these are lightweight guards in case the
    // framework fires update() during deserialization.
    // ------------------------------------------------------------------
    void presetWillBeLoaded() override { disableUpdate = true; }
    void presetHasLoaded()    override { disableUpdate = false; }

    // ------------------------------------------------------------------
    // update() — rebuild geometry from the shared simulator buffer.
    // We always rebuild when the buffer is non-empty (live laser data
    // changes every frame); when it IS empty we still call renderToFbo()
    // so the gizmos (floor grid, axes) continue to show.
    // ------------------------------------------------------------------
    void update(ofEventArgs& /*args*/) override {
        if (disableUpdate) return;
        const vector<float>& data = controller->getSimulatorData();
        rebuildGeometry(data);
        renderToFbo();
    }

    // ------------------------------------------------------------------
    // draw() — called by Oceanode inside the main ImGui frame.
    // Opens a standalone floating window when "Show Window" is true.
    // Mouse drag over the image orbits the easyCam; scroll wheel zooms.
    // ------------------------------------------------------------------
    void draw(ofEventArgs& /*args*/) override {
        if (!fbo.isAllocated()) return;
        if (!showWindow)        return;

        ImTextureID texId = (ImTextureID)(uintptr_t)fbo.getTexture().getTextureData().textureID;

        ImGui::SetNextWindowSize(ImVec2(600, 660), ImGuiCond_FirstUseEver);
        bool windowOpen = showWindow.get();
        if (ImGui::Begin("Laser Simulator##laserSimWin",
                         &windowOpen,
                         ImGuiWindowFlags_NoFocusOnAppearing)) {

            // ---- View mode buttons (perspective / top) ------------------
            // Highlight the active button with a tinted colour.
            ImVec4 activeCol  = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
            ImVec4 normalCol  = ImGui::GetStyle().Colors[ImGuiCol_Button];

            ImGui::PushStyleColor(ImGuiCol_Button,
                viewMode == 0 ? activeCol : normalCol);
            if (ImGui::SmallButton("[^]")) viewMode = 0;   // Perspective
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                viewMode == 1 ? activeCol : normalCol);
            if (ImGui::SmallButton("[||]")) viewMode = 1;   // Top / flat
            ImGui::PopStyleColor();

            // ---- FBO image — resize FBO when the window changes size ----
            float winW = ImGui::GetContentRegionAvail().x;
            float winH = ImGui::GetContentRegionAvail().y;
            float sz   = std::max(std::min(winW, winH), 100.0f);
            int   newSize = (int)sz;
            if (newSize != currentFboSize) {
                reallocFbo(newSize);
            }
            // Flip UV Y: oF FBO origin = bottom-left, ImGui expects top-left
            ImGui::Image(texId, ImVec2(sz, sz), ImVec2(0, 1), ImVec2(1, 0));

            // ---- Interactive input (perspective mode only) ---------------
            if (viewMode == 0 && ImGui::IsItemHovered()) {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.MouseDown[0]) {
                    camAzimuth   -= io.MouseDelta.x * 0.4f;
                    camElevation -= io.MouseDelta.y * 0.4f;
                    camElevation  = ofClamp(camElevation, -89.0f, 89.0f);
                }
                if (io.MouseWheel != 0.0f) {
                    camOrbDist -= io.MouseWheel * 0.5f;
                    camOrbDist  = std::max(camOrbDist, 0.5f);
                }
                glm::vec3 orbitCenter(0.0f, -(float)laserDistance * 0.5f, 0.0f);
                easyCam.orbit(camAzimuth, camElevation, camOrbDist, orbitCenter);
            }
        }
        ImGui::End();
        if (!windowOpen) showWindow = false;
    }

private:
    // ------------------------------------------------------------------
    // Constants
    // ------------------------------------------------------------------
    static constexpr int   STRIDE    = 5;  // x, y, r, g, b per vertex

    // ------------------------------------------------------------------
    // Coordinate remap (applied to every geometry output point):
    //   ILDA/logic space  →  world render space
    //   X                 →  X   (unchanged)
    //   old Y (ILDA pan)  →  new Z
    //   old Z (height/LD) →  new -Y  ← negate because ImGui UV flip
    //                                    (uv0=(0,1),uv1=(1,0)) inverts
    //                                    the displayed Y direction, so
    //                                    we pre-negate to make the laser
    //                                    head appear at the top visually.
    //
    // Floor (old Z=0) → new Y=0  (XZ plane)  — unchanged
    // Laser head (old (ox,oy,LD)) → new (ox, -LD, oy)  — at visual top
    // ------------------------------------------------------------------
    static glm::vec3 swapYZ(const glm::vec3& v) {
        return glm::vec3(v.x, -v.z, v.y);
    }

    // ------------------------------------------------------------------
    // Helper: is the vertex at `idx` an isolated single point?
    // (next entry in buffer is -1 or we're at the end)
    // ------------------------------------------------------------------
    bool isSinglePoint(const vector<float>& data, int idx) const {
        int next = idx + STRIDE;
        if (next >= (int)data.size()) return true;
        return (data[next] == -1.0f);
    }

    // ------------------------------------------------------------------
    // Projection: normalised laser angles → 3D floor-hit point
    //
    // The laser head (origin) is at (offsetX, offsetY, laserDistance).
    // Beams fire DOWNWARD (−Z) with angular spread from FOV_X / FOV_Y.
    //
    //  Planar (Spherical Projection = false):
    //    Beam direction is tilted by angX/angY from straight-down (−Z).
    //    The beam intersects the Z=0 floor plane at:
    //      hit.x = offsetX + angX * laserDistance
    //      hit.y = offsetY + angY * laserDistance
    //      hit.z = 0
    //
    //  Spherical dome (Spherical Projection = true):
    //    Direction vector tilted angX/angY from −Z axis; intersect Z=0:
    //      dir = normalize(sin(angX)*cos(angY), cos(angX)*sin(angY), −cos(angX)*cos(angY))
    //      t   = laserDistance / |dir.z|   (distance to Z=0 from laser head)
    //      hit = origin + dir * t
    // ------------------------------------------------------------------
    glm::vec3 computeProjected(float angX, float angY) const {
        float LD  = laserDistance;
        float ox  = (float)offsetX;
        float oy  = (float)offsetY;
        if (sphericalProjection) {
            // Two-axis galvo model: X mirror rotates around Y, then Y mirror
            // rotates around the (already-tilted) X axis.
            // dir = Ry(angX) * Rx(angY) * (0,0,-1)
            // → already a unit vector; normalize() kept as safety guard.
            glm::vec3 dir( sinf(angX),
                           sinf(angY) * cosf(angX),
                          -cosf(angX) * cosf(angY));
            dir = glm::normalize(dir);
            float t = (fabsf(dir.z) > 1e-6f) ? LD / fabsf(dir.z) : LD;
            return swapYZ(glm::vec3(ox, oy, LD) + dir * t);
        } else {
            // Planar: beam tilted angX/angY from straight-down (-Z).
            // Floor hit = origin + tan(angle) * LD per axis.
            // Must use tan() so that corners (±halfFOV) reach the frustum
            // boundary computed in drawFloorGrid() with tan(halfFOV)*LD.
            return swapYZ(glm::vec3(ox + tanf(angX) * LD,
                                    oy + tanf(angY) * LD,
                                    0.0f));
        }
    }

    // ------------------------------------------------------------------
    // Apply brightness multiplier to a color (clamped to [0,1]).
    // Used in RenderLikeUnity mode; skipped in self-illuminated mode.
    // ------------------------------------------------------------------
    ofFloatColor applyBrightness(const ofFloatColor& c) const {
        float b = brightness;
        return ofFloatColor(std::min(c.r * b, 1.0f),
                            std::min(c.g * b, 1.0f),
                            std::min(c.b * b, 1.0f),
                            std::min(0.52f * b, 1.0f)); // Unity _Alpha=0.52
    }

    // ------------------------------------------------------------------
    // Add a cylinder from `origin` to `proj`, capped with a disc at proj.
    // Used for single isolated laser dots.
    // ------------------------------------------------------------------
    void addCylinder(ofMesh& mesh,
                     const glm::vec3& origin, const glm::vec3& proj,
                     const ofFloatColor& col) const {
        int   cylRes = pointCylinderRes;
        float cylR   = pointCylinderSize * 0.5f;

        // Beam axis direction (normalised)
        glm::vec3 axis = glm::normalize(proj - origin);
        // Build two perpendicular vectors to the axis
        glm::vec3 arbitrary = (fabsf(axis.z) < 0.9f)
                                ? glm::vec3(0, 0, 1)
                                : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(axis, arbitrary));
        glm::vec3 up    = glm::cross(axis, right);

        // ---- Cylinder wall (quad strip around the tube) ----
        for (int i = 0; i < cylRes; ++i) {
            float a0 = (float)i       / (float)cylRes * TWO_PI;
            float a1 = (float)(i + 1) / (float)cylRes * TWO_PI;

            glm::vec3 off0 = (right * cosf(a0) + up * sinf(a0)) * cylR;
            glm::vec3 off1 = (right * cosf(a1) + up * sinf(a1)) * cylR;

            uint32_t vi = (uint32_t)mesh.getNumVertices();
            mesh.addVertex(origin + off0);  mesh.addColor(col);  // 0 near-left
            mesh.addVertex(proj   + off0);  mesh.addColor(col);  // 1 far-left
            mesh.addVertex(origin + off1);  mesh.addColor(col);  // 2 near-right
            mesh.addVertex(proj   + off1);  mesh.addColor(col);  // 3 far-right

            mesh.addIndex(vi+1); mesh.addIndex(vi+0); mesh.addIndex(vi+2);
            mesh.addIndex(vi+2); mesh.addIndex(vi+3); mesh.addIndex(vi+1);
        }
        // Floor-plane disc is handled separately by addFloorDisc() in Pass 2.
    }

    // ------------------------------------------------------------------
    // Add a floor disc at the given center (on the Z=0 plane)
    // Used in the floor projection pass for single laser dots.
    // ------------------------------------------------------------------
    void addFloorDisc(ofMesh& mesh,
                      const glm::vec3& center,
                      const ofFloatColor& col) const {
        int   res = pointProjectionRes;
        float r   = pointProjectionSize * 0.5f;

        uint32_t centerIdx = (uint32_t)mesh.getNumVertices();
        mesh.addVertex(center); mesh.addColor(col);

        // Floor disc lies on Y=0 plane → spread in X and Z
        for (int i = 0; i <= res; ++i) {
            float a = (float)i / (float)res * TWO_PI;
            glm::vec3 off(cosf(a) * r, 0.0f, sinf(a) * r);
            mesh.addVertex(center + off); mesh.addColor(col);
        }
        for (int i = 0; i < res; ++i) {
            uint32_t va = centerIdx + 1 + i;
            uint32_t vb = centerIdx + 1 + (i + 1) % res;
            mesh.addIndex(centerIdx);
            mesh.addIndex(va);
            mesh.addIndex(vb);
        }
    }

    // ------------------------------------------------------------------
    // Main geometry rebuild — port of Unity LaserGeomCpu::UpdateData()
    // Two passes:
    //   Pass 1 → beamMesh  (cylinder beams from Z=0 origin → projected point)
    //   Pass 2 → floorMesh (thickened quads + discs on the Z=0 floor plane)
    // ------------------------------------------------------------------
    void rebuildGeometry(const vector<float>& data) {
        beamMesh.clear();
        floorMesh.clear();

        if (data.empty()) return;

        beamMesh.setMode(OF_PRIMITIVE_TRIANGLES);
        floorMesh.setMode(OF_PRIMITIVE_TRIANGLES);

        float aX = ofDegToRad(fovX) * 0.5f;
        float aY = ofDegToRad(fovY) * 0.5f;
        float ox = ofDegToRad(offsetX);
        float oy = ofDegToRad(offsetY);
        float NW = nearWidth;
        bool  lu = renderLikeUnity;

        // ================================================================
        // Pass 1: beam mesh
        // ================================================================
        {
            bool     bFirst   = true;
            uint32_t quadIdx  = 0;
            int      vertBase = 0;  // vertex offset at start of current polyline

            for (int idx = 0; idx < (int)data.size(); ) {
                if (data[idx] == -1.0f) {
                    bFirst  = true;
                    quadIdx = 0;
                    ++idx;
                    continue;
                }

                float p0X = data[idx + 0] * 2.0f - 1.0f;
                float p0Y = data[idx + 1] * 2.0f - 1.0f;
                ofFloatColor raw(data[idx+2], data[idx+3], data[idx+4], 1.0f);
                ofFloatColor col = lu ? applyBrightness(raw) : raw;

                float angX = p0X * aX + ox;
                float angY = p0Y * aY + oy;

                glm::vec3 proj   = computeProjected(angX, angY);
                // Laser head at (offsetX, offsetY, laserDistance) in pre-swap coords
                // → swapYZ → (offsetX, laserDistance, offsetY) in render coords
                glm::vec3 origin = swapYZ(glm::vec3((float)offsetX + p0X * NW,
                                                    (float)offsetY + p0Y * NW,
                                                    (float)laserDistance));

                bool bSingle = isSinglePoint(data, idx);

                if (bFirst && bSingle) {
                    // Isolated point: full cylinder + disc
                    addCylinder(beamMesh, origin, proj, col);
                    bFirst  = true;
                    quadIdx = 0;
                    idx    += STRIDE;
                    continue;
                }

                // Quad strip segment (2 verts: origin + projected)
                uint32_t vi = (uint32_t)beamMesh.getNumVertices();
                beamMesh.addVertex(origin); beamMesh.addColor(col);
                beamMesh.addVertex(proj);   beamMesh.addColor(col);

                if (!bFirst && quadIdx >= 1) {
                    // Connect to previous pair with two triangles
                    uint32_t a = vi - 2, b = vi - 1;
                    uint32_t c = vi,     d = vi + 1;
                    beamMesh.addIndex(b); beamMesh.addIndex(a); beamMesh.addIndex(c);
                    beamMesh.addIndex(c); beamMesh.addIndex(d); beamMesh.addIndex(b);
                }

                ++quadIdx;
                bFirst = false;
                idx   += STRIDE;
            }
        }

        // ================================================================
        // Pass 2: floor projection mesh (Z = 0 plane)
        // ================================================================
        {
            bool bFirst = true;

            for (int idx = 0; idx < (int)data.size(); ) {
                if (data[idx] == -1.0f) {
                    bFirst = true;
                    ++idx;
                    continue;
                }

                float p0X = data[idx + 0] * 2.0f - 1.0f;
                float p0Y = data[idx + 1] * 2.0f - 1.0f;
                ofFloatColor raw(data[idx+2], data[idx+3], data[idx+4], 1.0f);
                ofFloatColor col = lu ? applyBrightness(raw) : raw;

                float angX = p0X * aX + ox;
                float angY = p0Y * aY + oy;

                glm::vec3 proj = computeProjected(angX, angY);
                // After swapYZ: floor is Y=0. Force Y=0, keep X and Z.
                glm::vec3 fp(proj.x, 0.0f, proj.z);

                bool bSingle = isSinglePoint(data, idx);

                if (bFirst && bSingle) {
                    // Single dot: a disc on the floor
                    addFloorDisc(floorMesh, fp, col);
                    bFirst = true;
                    idx   += STRIDE;
                    continue;
                }

                // Segment: check whether the next vertex is also a segment
                // (not a -1). Build a thickened quad on the floor.
                if (idx < (int)data.size() - STRIDE && data[idx + STRIDE] != -1.0f) {
                    float p1X = data[idx + STRIDE + 0] * 2.0f - 1.0f;
                    float p1Y = data[idx + STRIDE + 1] * 2.0f - 1.0f;
                    ofFloatColor raw1(data[idx+STRIDE+2],data[idx+STRIDE+3],data[idx+STRIDE+4],1.0f);
                    ofFloatColor col1 = lu ? applyBrightness(raw1) : raw1;

                    float a1X = p1X * aX + ox;
                    float a1Y = p1Y * aY + oy;
                    glm::vec3 proj1 = computeProjected(a1X, a1Y);
                    glm::vec3 fp1(proj1.x, 0.0f, proj1.z);  // floor is Y=0

                    // Perpendicular in the XZ floor plane (Y=0)
                    glm::vec2 seg   = glm::vec2(fp1.x - fp.x, fp1.z - fp.z);
                    float     segLen = glm::length(seg);
                    if (segLen > 1e-6f) {
                        glm::vec2 perp = glm::normalize(glm::vec2(-seg.y, seg.x))
                                         * (float)floorThickness;

                        uint32_t vi = (uint32_t)floorMesh.getNumVertices();

                        // 4 vertices on floor (Y=0): perp in X and Z
                        floorMesh.addVertex(glm::vec3(fp.x  - perp.x, 0, fp.z  - perp.y));
                        floorMesh.addColor(col);
                        floorMesh.addVertex(glm::vec3(fp.x  + perp.x, 0, fp.z  + perp.y));
                        floorMesh.addColor(col);
                        floorMesh.addVertex(glm::vec3(fp1.x - perp.x, 0, fp1.z - perp.y));
                        floorMesh.addColor(col1);
                        floorMesh.addVertex(glm::vec3(fp1.x + perp.x, 0, fp1.z + perp.y));
                        floorMesh.addColor(col1);

                        floorMesh.addIndex(vi+1); floorMesh.addIndex(vi+0); floorMesh.addIndex(vi+2);
                        floorMesh.addIndex(vi+2); floorMesh.addIndex(vi+3); floorMesh.addIndex(vi+1);
                    }
                }

                bFirst = false;
                idx   += STRIDE;
            }
        }
    }

    // ------------------------------------------------------------------
    // Helper: project a polyline defined in angle-space through
    // computeProjected() and draw it as a series of line segments.
    // Resolution is controlled by the caller via the `samples` parameter
    // (exposed to the user as the "Grid Curve Res" ofParameter).
    // ------------------------------------------------------------------
    void drawProjectedLine(float ax0, float ay0,
                           float ax1, float ay1,
                           int   samples) const {
        glm::vec3 prev = computeProjected(ax0, ay0);
        for (int s = 1; s <= samples; ++s) {
            float t  = (float)s / (float)samples;
            float ax = ax0 + (ax1 - ax0) * t;
            float ay = ay0 + (ay1 - ay0) * t;
            glm::vec3 curr = computeProjected(ax, ay);
            ofDrawLine(prev, curr);
            prev = curr;
        }
    }

    // ------------------------------------------------------------------
    // Floor grid: 4 × 4 cells.
    //
    // Planar mode: lines are perfectly straight, drawn as two direct
    //   ofDrawLine() calls per axis — the grid is a clean square.
    //
    // Spherical mode: lines are defined in angle-space and projected
    //   through computeProjected() so galvo barrel distortion is shown.
    //   Vertical lines (constant angX) bow outward; horizontal rows
    //   (constant angY) stay straight. Resolution = "Grid Curve Res".
    // ------------------------------------------------------------------
    void drawFloorGrid() const {
        const int N = 5;   // 5 lines per axis → 4 cells

        ofSetColor(60, 60, 80, 200);
        ofSetLineWidth(1.0f);

        if (!sphericalProjection) {
            // Planar: simple square grid at tan(halfFOV)*LD
            float LD    = (float)laserDistance;
            float halfX = tanf(ofDegToRad((float)fovX * 0.5f)) * LD;
            float halfZ = tanf(ofDegToRad((float)fovY * 0.5f)) * LD;
            for (int i = 0; i < N; ++i) {
                float tx = ofMap(i, 0, N - 1, -halfX, halfX);
                float tz = ofMap(i, 0, N - 1, -halfZ, halfZ);
                ofDrawLine(glm::vec3(-halfX, 0, tz), glm::vec3(halfX, 0, tz));
                ofDrawLine(glm::vec3(tx, 0, -halfZ), glm::vec3(tx, 0,  halfZ));
            }
        } else {
            // Spherical: sample through computeProjected() to show curvature
            int   res = gridCurveRes;
            float aX  = ofDegToRad((float)fovX) * 0.5f;
            float aY  = ofDegToRad((float)fovY) * 0.5f;
            float ox  = ofDegToRad((float)offsetX);
            float oy  = ofDegToRad((float)offsetY);
            float minAX = ox - aX,  maxAX = ox + aX;
            float minAY = oy - aY,  maxAY = oy + aY;

            for (int i = 0; i < N; ++i) {
                float ay = ofMap(i, 0, N - 1, minAY, maxAY);
                drawProjectedLine(minAX, ay, maxAX, ay, res);  // horizontal row
            }
            for (int i = 0; i < N; ++i) {
                float ax = ofMap(i, 0, N - 1, minAX, maxAX);
                drawProjectedLine(ax, minAY, ax, maxAY, res);  // vertical column
            }
        }
    }

    // ------------------------------------------------------------------
    // Frustum outline: boundary of the laser FOV on the floor.
    //
    // Planar mode: a simple rectangle — 4 straight ofDrawLine() calls.
    //
    // Spherical mode: 4 edges sampled through computeProjected() so the
    //   left/right edges visibly bow outward (real galvo behaviour).
    // ------------------------------------------------------------------
    void drawFrustumOutline() const {
        ofSetColor(180, 180, 80, 220);
        ofSetLineWidth(2.0f);

        if (!sphericalProjection) {
            // Planar: simple rectangle
            float LD    = (float)laserDistance;
            float halfX = tanf(ofDegToRad((float)fovX * 0.5f)) * LD;
            float halfZ = tanf(ofDegToRad((float)fovY * 0.5f)) * LD;
            ofDrawLine(glm::vec3(-halfX, 0, -halfZ), glm::vec3( halfX, 0, -halfZ)); // bottom
            ofDrawLine(glm::vec3(-halfX, 0,  halfZ), glm::vec3( halfX, 0,  halfZ)); // top
            ofDrawLine(glm::vec3(-halfX, 0, -halfZ), glm::vec3(-halfX, 0,  halfZ)); // left
            ofDrawLine(glm::vec3( halfX, 0, -halfZ), glm::vec3( halfX, 0,  halfZ)); // right
        } else {
            // Spherical: sample each edge through computeProjected()
            int   res = gridCurveRes;
            float aX  = ofDegToRad((float)fovX) * 0.5f;
            float aY  = ofDegToRad((float)fovY) * 0.5f;
            float ox  = ofDegToRad((float)offsetX);
            float oy  = ofDegToRad((float)offsetY);
            float minAX = ox - aX,  maxAX = ox + aX;
            float minAY = oy - aY,  maxAY = oy + aY;

            drawProjectedLine(minAX, minAY, maxAX, minAY, res); // bottom
            drawProjectedLine(minAX, maxAY, maxAX, maxAY, res); // top
            drawProjectedLine(minAX, minAY, minAX, maxAY, res); // left
            drawProjectedLine(maxAX, minAY, maxAX, maxAY, res); // right
        }
    }

    // ------------------------------------------------------------------
    // XYZ axis gizmo  X=red, Y=green, Z=blue
    // After swapYZ with negation, height (old Z) maps to -Y in render space,
    // but appears as visual-UP after the ImGui UV flip.
    // Draw the blue (height) line toward -Y so it visually points upward.
    // ------------------------------------------------------------------
    void drawAxes() const {
        const float len = 1.5f;
        ofSetLineWidth(2.0f);
        ofSetColor(ofColor::red);
        ofDrawLine(glm::vec3(0,0,0), glm::vec3( len, 0,    0));  // X → right
        ofSetColor(ofColor::green);
        ofDrawLine(glm::vec3(0,0,0), glm::vec3(0,    0,  len));  // old Y → +Z
        ofSetColor(ofColor::blue);
        ofDrawLine(glm::vec3(0,0,0), glm::vec3(0, -len,    0));  // old Z → -Y = visual up
    }

    // ------------------------------------------------------------------
    // Render the 3D scene into the FBO
    // ------------------------------------------------------------------
    void renderToFbo() {
        fbo.begin();
        ofClear(0, 0, 0, 255);

        if (viewMode == 1) {
			renderLikeUnity = true;
            // ---- Top view: use easyCam with orbit() straight above floor ---
            // orbitAz=0, orbitEl=90 puts the camera directly above (0, Y, 0).
            // This reuses the proven easyCam path used by Perspective mode.
            float LD  = (float)laserDistance;

            // Temporarily override easyCam to a top-down position and distance
            float savedAz  = camAzimuth;
            float savedEl  = camElevation;
            float savedDist = camOrbDist;

            glm::vec3 orbitCenter(0.0f, -LD * 0.5f, 0.0f);
            easyCam.orbit(0.01f, 89.99f, LD * 0.50f, orbitCenter);

            easyCam.begin(ofRectangle(0, 0, currentFboSize, currentFboSize));

            ofEnableDepthTest();
            if (drawFloor)   drawFloorGrid();
            if (drawAxis)    drawAxes();
            if (drawFrustum) drawFrustumOutline();

            // Top view: only floor mesh (laser footprint on the floor)
            if (renderLikeUnity) {
                ofDisableDepthTest();
                ofEnableAlphaBlending();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                ofSetColor(255);
                floorMesh.draw();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                ofEnableDepthTest();
            } else {
                ofDisableLighting();
                ofSetColor(255);
                floorMesh.draw();
            }

            easyCam.end();
            // Restore perspective cam orbit for next perspective frame
            easyCam.orbit(savedAz, savedEl, savedDist, orbitCenter);
        } else {
			renderLikeUnity = true;
            // ---- Perspective view: easyCam orbit -------------------------
            easyCam.begin(ofRectangle(0, 0, currentFboSize, currentFboSize));

        ofEnableDepthTest();

        if (drawFloor)   drawFloorGrid();
        if (drawAxis)    drawAxes();
        if (drawFrustum) drawFrustumOutline();

        if (renderLikeUnity) {
            // ---------------------------------------------------
            // RenderLikeUnity = true
            //   Additive transparent blending: beams accumulate
            //   brightness, matching the glowing laser look.
            //   Brightness is already baked into vertex alpha
            //   (applyBrightness sets alpha = 0.52 * brightness).
            //   Depth write off so overlapping beams add up.
            // ---------------------------------------------------
            ofDisableDepthTest();
            ofEnableAlphaBlending();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive
            ofSetColor(255, 255, 255, 255);

            beamMesh.draw();
            floorMesh.draw();

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // restore
            ofEnableDepthTest();

        } else {
            // ---------------------------------------------------
            // RenderLikeUnity = false
            //   Self-illuminated: raw vertex colours, no shading,
            //   no blending tricks.
            // ---------------------------------------------------
            ofDisableLighting();
            ofSetColor(255, 255, 255, 255);

            beamMesh.draw();
            floorMesh.draw();
        }

        easyCam.end();
        } // end else (perspective view)

        fbo.end();
        if (fbo.isAllocated()) textureOut = &fbo.getTexture();
    }

    // ------------------------------------------------------------------
    // Helper: allocate (or reallocate) the FBO at the given square size.
    // Clears to black after allocation.
    // ------------------------------------------------------------------
    void reallocFbo(int size) {
        currentFboSize = size;
        ofFboSettings fboSettings;
        fboSettings.width          = size;
        fboSettings.height         = size;
        fboSettings.internalformat = GL_RGBA;
        fboSettings.useDepth       = true;
        fboSettings.useStencil     = false;
        fboSettings.numSamples     = 8;
        fbo.allocate(fboSettings);
        fbo.begin();
        ofClear(0, 0, 0, 255);
        fbo.end();
    }

    // ------------------------------------------------------------------
    // Data members
    // ------------------------------------------------------------------
    shared_ptr<ildaController> controller;

    ofFbo      fbo;
    int        currentFboSize = 1024;  // tracks current square FBO dimension
    ofVboMesh  beamMesh;   // GPU-resident for faster draw calls each frame
    ofVboMesh  floorMesh;

    // View mode: 0 = Perspective (easyCam orbit), 1 = Top orthographic
    int        viewMode = 0;

    // Parameters — projection
    ofParameter<bool>  sphericalProjection;
    ofParameter<float> fovX;
    ofParameter<float> fovY;
    ofParameter<float> nearWidth;
    ofParameter<float> offsetX;
    ofParameter<float> offsetY;
    ofParameter<float> laserDistance;

    // Parameters — geometry detail
    ofParameter<float> floorThickness;
    ofParameter<float> pointCylinderSize;
    ofParameter<float> pointProjectionSize;
    ofParameter<int>   pointCylinderRes;
    ofParameter<int>   pointProjectionRes;
    ofParameter<int>   gridCurveRes;

    // Camera — ofEasyCam with manual ImGui orbit input
    ofEasyCam  easyCam;
    float      camAzimuth   =   0.0f;   // degrees, accumulated from mouse drag
    float      camElevation =  30.0f;   // degrees, clamped to ±89
    float      camOrbDist   =  15.0f;   // world units (separate from ofParameter)

    // Parameters — camera clip planes
    ofParameter<float> camNear;
    ofParameter<float> camFar;

    // Parameter listeners
    ofEventListeners parameterListeners;

    // Parameters — gizmos
    ofParameter<bool>  drawFloor;
    ofParameter<bool>  drawAxis;
    ofParameter<bool>  drawFrustum;

    // Parameters — window
    ofParameter<bool>  showWindow;

    // Parameters — output
    ofParameter<ofTexture*> textureOut;

    // Parameters — rendering
    ofParameter<bool>  renderLikeUnity;
    ofParameter<float> brightness;

    // Polish / guard state
    bool   disableUpdate = false;
};

#endif /* laserSimulator_h */
