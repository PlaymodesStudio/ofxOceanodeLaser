//
//  ildaMask.h
//  ofxOceanodeLaser
//
//  Opacity mask applied to every ILDA Shape before it reaches the projector, the
//  ILDA Debugger and the Laser Simulator. White (1) keeps the colour, black (0) fades
//  it to black. The mask is a parametric shape, a texture, or both multiplied.
//
//  The texture is shrunk on the GPU to a small RGBA FBO and copied into a pixel buffer
//  object; the CPU reads that buffer one or more frames later, once the copy has
//  finished, so the app never waits on the GPU. Several ILDA Mask nodes multiply.
//

#ifndef ildaMask_h
#define ildaMask_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"
#include "ildaMaskField.h"

class ildaMask : public ofxOceanodeNodeModel {
public:
    ildaMask(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Mask"){}

    ~ildaMask(){
        controller->removeMask(&field);
        releaseReadback();
    }

    std::string macroMixerBlocker() const override { return "Writes the shared ILDA mask. Keep ILDA Mask outside MacroMixer."; }

    void setup(){
        addParameterDropdown(shape, "Shape", 2, {"None", "Linear", "Ellipse", "Rect"});
        addParameter(x.set("X", {0.5}, {0}, {1}));
        addParameter(y.set("Y", {0.5}, {0}, {1}));
        addParameter(w.set("W", {0.5}, {0}, {2}));
        addParameter(h.set("H", {0.5}, {0}, {2}));
        addParameter(angle.set("Angle", {0}, {-180}, {180}));
        addParameter(feather.set("Feather", {0.1}, {0}, {1}));
        addParameter(invert.set("Invert", false));
        addParameter(amount.set("Amount", 1, 0, 1));
        addParameter(texture.set("Texture", nullptr, nullptr, nullptr));
        addParameterDropdown(channel, "Channel", 0, {"Luma", "Red", "Green", "Blue", "Alpha"});
        addOutputParameter(preview.set("Mask", nullptr, nullptr, nullptr));
        addInspectorParameter(textureResolution.set("Tex Res", 128, 16, 512));
        addInspectorParameter(flipY.set("Flip Y", false));

        listeners.push(shape.newListener([this](int &){ refreshShapes(); }));
        listeners.push(x.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(y.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(w.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(h.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(angle.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(feather.newListener([this](vector<float> &){ refreshShapes(); }));
        listeners.push(invert.newListener([this](bool &){ refreshShapes(); }));
        listeners.push(amount.newListener([this](float &){ refreshShapes(); }));
        listeners.push(channel.newListener([this](int &){ previewDirty = true; }));
        listeners.push(flipY.newListener([this](bool &){ previewDirty = true; }));

        refreshShapes();
        controller->addMask(&field);
    }

    void update(ofEventArgs &a) override {
        ofTexture *tex = texture.get();
        if(tex != nullptr && tex->isAllocated()){
            collectReadback();
            issueReadback(*tex);
        }
        else if(field.useGrid || !slots.empty()){
            releaseReadback();
            field.useGrid = false;
            previewDirty = true;
        }

        if(previewDirty){
            updatePreview();
            previewDirty = false;
        }
    }

private:
    // One in-flight GPU -> CPU copy of the shrunk texture.
    struct ReadbackSlot {
        ofBufferObject buffer;
        GLsync fence = 0;
        bool pending = false;
        uint64_t frame = 0;
        int resolution = 0;
    };

    void refreshShapes(){
        field.shape = static_cast<ildaMasking::Shape>(shape.get());
        field.invert = invert;
        field.amount = amount;

        const vector<float> *columns[] = {&x.get(), &y.get(), &w.get(), &h.get(), &angle.get(), &feather.get()};
        size_t count = 1;
        for(auto column : columns) count = std::max(count, column->size());
        // A shorter vector repeats its last value, so a single Feather applies to every shape.
        auto at = [](const vector<float> &v, size_t i, float fallback){
            if(v.empty()) return fallback;
            return v[std::min(i, v.size() - 1)];
        };
        field.shapes.resize(count);
        for(size_t i = 0; i < count; i++){
            auto &s = field.shapes[i];
            s.x = at(x, i, 0.5f);
            s.y = at(y, i, 0.5f);
            s.w = at(w, i, 0.5f);
            s.h = at(h, i, 0.5f);
            s.angle = at(angle, i, 0.0f);
            s.feather = at(feather, i, 0.0f);
        }
        field.prepare();
        previewDirty = true;
    }

    static bool fencesSupported(){
#ifdef TARGET_OPENGLES
        return false;
#else
        return GLEW_VERSION_3_2 || GLEW_ARB_sync;
#endif
    }

    // Reads every finished copy, oldest first, so the grid ends with the newest one.
    void collectReadback(){
        vector<ReadbackSlot*> ready;
        for(auto &slot : slots){
            if(!slot.pending) continue;
            bool finished;
            if(slot.fence != 0){
                GLenum status = glClientWaitSync(slot.fence, 0, 0);
                finished = status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED;
            }
            else{
                // Without fences, a copy issued on an earlier frame has almost always finished.
                finished = slot.frame < ofGetFrameNum();
            }
            if(finished) ready.push_back(&slot);
        }
        std::sort(ready.begin(), ready.end(), [](ReadbackSlot *a, ReadbackSlot *b){ return a->frame < b->frame; });
        for(auto slot : ready) readSlot(*slot);
    }

    void readSlot(ReadbackSlot &slot){
        if(slot.fence != 0){
            glDeleteSync(slot.fence);
            slot.fence = 0;
        }
        slot.pending = false;

        const int res = slot.resolution;
        slot.buffer.bind(GL_PIXEL_PACK_BUFFER);
        const unsigned char *rgba = static_cast<const unsigned char*>(slot.buffer.map(GL_READ_ONLY));
        if(rgba != nullptr){
            auto &grid = field.grid;
            grid.width = res;
            grid.height = res;
            grid.values.resize(res * res);
            const int mode = channel;
            for(int row = 0; row < res; row++){
                const unsigned char *src = rgba + (size_t)(flipY ? res - 1 - row : row) * res * 4;
                float *dst = grid.values.data() + (size_t)row * res;
                for(int col = 0; col < res; col++, src += 4){
                    float value;
                    switch(mode){
                        case 1: value = src[0]; break;
                        case 2: value = src[1]; break;
                        case 3: value = src[2]; break;
                        case 4: value = src[3]; break;
                        default: value = 0.2126f * src[0] + 0.7152f * src[1] + 0.0722f * src[2]; break;
                    }
                    dst[col] = value / 255.0f;
                }
            }
            slot.buffer.unmap();
            field.useGrid = true;
            previewDirty = true;
        }
        slot.buffer.unbind(GL_PIXEL_PACK_BUFFER);
    }

    // Shrinks the texture into the FBO and starts copying it into a free slot. When both
    // slots are still in flight this frame is skipped instead of waiting for the GPU.
    void issueReadback(ofTexture &tex){
        const int res = textureResolution;
        if(!fbo.isAllocated() || fbo.getWidth() != res){
            ofFboSettings settings;
            settings.width = res;
            settings.height = res;
            settings.internalformat = GL_RGBA;
            settings.useDepth = false;
            settings.useStencil = false;
            settings.numSamples = 0;
            fbo.allocate(settings);
        }
        if(slots.empty()) slots.resize(2);

        ReadbackSlot *slot = nullptr;
        for(auto &s : slots){
            if(!s.pending){ slot = &s; break; }
        }
        if(slot == nullptr) return;

        // Blending off so the alpha channel arrives untouched for the "Alpha" channel.
        fbo.begin();
        ofPushStyle();
        ofClear(0, 0, 0, 0);
        ofDisableBlendMode();
        ofDisableDepthTest();
        ofSetColor(255);
        tex.draw(0, 0, res, res);
        ofPopStyle();
        fbo.end();

        const GLsizeiptr bytes = (GLsizeiptr)res * res * 4;
        if(!slot->buffer.isAllocated() || slot->resolution != res){
            slot->buffer.allocate(bytes, GL_STREAM_READ);
            slot->resolution = res;
        }
        fbo.getTexture().copyTo(slot->buffer);
        slot->fence = fencesSupported() ? glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0) : 0;
        slot->pending = true;
        slot->frame = ofGetFrameNum();
    }

    void releaseReadback(){
        for(auto &slot : slots){
            if(slot.fence != 0) glDeleteSync(slot.fence);
        }
        slots.clear();
    }

    // Draws the combined mask of this node into the "Mask" output so it can be inspected.
    void updatePreview(){
        if(!previewPixels.isAllocated()){
            previewPixels.allocate(previewSize, previewSize, OF_PIXELS_RGBA);
        }
        auto pixels = previewPixels.getData();
        for(int row = 0; row < previewSize; row++){
            for(int col = 0; col < previewSize; col++){
                const float value = field.sample((col + 0.5f) / previewSize, (row + 0.5f) / previewSize);
                const unsigned char byte = (unsigned char)std::round(glm::clamp(value, 0.0f, 1.0f) * 255.0f);
                unsigned char *p = pixels + ((size_t)row * previewSize + col) * 4;
                p[0] = p[1] = p[2] = byte;
                p[3] = 255;
            }
        }
        if(!previewTexture.isAllocated()){
            previewTexture.allocate(previewPixels);
        }
        previewTexture.loadData(previewPixels);
        if(preview.get() != &previewTexture){
            preview = &previewTexture;
        }
    }

    shared_ptr<ildaController> controller;
    ildaMasking::Field field;

    ofParameter<int> shape;
    ofParameter<vector<float>> x;
    ofParameter<vector<float>> y;
    ofParameter<vector<float>> w;
    ofParameter<vector<float>> h;
    ofParameter<vector<float>> angle;
    ofParameter<vector<float>> feather;
    ofParameter<bool> invert;
    ofParameter<float> amount;
    ofParameter<ofTexture*> texture;
    ofParameter<int> channel;
    ofParameter<ofTexture*> preview;
    ofParameter<int> textureResolution;
    ofParameter<bool> flipY;
    ofEventListeners listeners;

    ofFbo fbo;
    vector<ReadbackSlot> slots;

    static constexpr int previewSize = 128;
    ofPixels previewPixels;
    ofTexture previewTexture;
    bool previewDirty = true;
};

#endif /* ildaMask_h */
