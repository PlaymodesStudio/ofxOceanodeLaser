//
//  ildaMaskField.h
//  ofxOceanodeLaser
//
//  Opacity mask math for laser shapes. A mask maps a canvas position (u, v in 0..1,
//  y down, same space as the 800x800 laser canvas divided by 800) to a gain in 0..1:
//  1 keeps the shape colour, 0 fades it to black.
//
//  This header only depends on glm and the standard library so tests can build it
//  without openFrameworks. The ILDA Mask node fills a Field, ildaController combines
//  every registered Field, and ildaShape resamples each polyline with maskPolyline().
//

#ifndef ildaMaskField_h
#define ildaMaskField_h

#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

namespace ildaMasking {

enum class Shape {
    None = 0,   // no parametric shape: the mask is 1 everywhere (texture only)
    Linear,     // half plane through (x, y): passes on the negative side of the rotated x axis
    Ellipse,    // ellipse centred on (x, y) with full size (w, h)
    Rect        // rectangle centred on (x, y) with full size (w, h)
};

struct ShapeParams {
    float x = 0.5f;
    float y = 0.5f;
    float w = 0.5f;
    float h = 0.5f;
    float angle = 0.0f;   // degrees, rotates the shape around (x, y)
    float feather = 0.1f; // width of the fade, measured inwards from the shape edge
};

// Signed distance to the shape edge in canvas units: negative inside, positive outside.
// `rotation` is (cos, sin) of the shape angle, cached by Field::prepare().
inline float signedDistance(Shape shape, const ShapeParams& s, glm::vec2 rotation, float u, float v) {
    const float c = rotation.x;
    const float sn = rotation.y;
    const glm::vec2 d(u - s.x, v - s.y);
    const glm::vec2 p(c * d.x + sn * d.y, -sn * d.x + c * d.y);

    switch (shape) {
        case Shape::Linear:
            return p.x;
        case Shape::Ellipse: {
            const glm::vec2 r(std::max(s.w * 0.5f, 1e-6f), std::max(s.h * 0.5f, 1e-6f));
            // Inigo Quilez's ellipse distance approximation; exact on the axes.
            const float k0 = glm::length(p / r);
            if (k0 < 1e-6f) return -std::min(r.x, r.y);
            const float k1 = glm::length(p / (r * r));
            return k0 * (k0 - 1.0f) / k1;
        }
        case Shape::Rect: {
            const glm::vec2 q = glm::abs(p) - glm::vec2(std::max(s.w, 0.0f), std::max(s.h, 0.0f)) * 0.5f;
            return glm::length(glm::max(q, glm::vec2(0.0f))) + std::min(std::max(q.x, q.y), 0.0f);
        }
        case Shape::None:
        default:
            return -1.0f;
    }
}

inline glm::vec2 rotationOf(const ShapeParams& s) {
    const float radians = glm::radians(s.angle);
    return glm::vec2(std::cos(radians), std::sin(radians));
}

// 1 inside the shape, 0 outside, smoothstep fade over `feather` inside the edge.
inline float evaluateShape(Shape shape, const ShapeParams& s, glm::vec2 rotation, float u, float v) {
    if (shape == Shape::None) return 1.0f;
    const float sd = signedDistance(shape, s, rotation, u, v);
    if (s.feather <= 0.0f) return sd <= 0.0f ? 1.0f : 0.0f;
    const float t = glm::clamp((sd + s.feather) / s.feather, 0.0f, 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

inline float evaluateShape(Shape shape, const ShapeParams& s, float u, float v) {
    return evaluateShape(shape, s, rotationOf(s), u, v);
}

// Low resolution mask read back from a texture. Row 0 is the top of the canvas (v = 0).
struct Grid {
    int width = 0;
    int height = 0;
    std::vector<float> values;

    bool empty() const { return width <= 0 || height <= 0 || (int)values.size() < width * height; }

    // Bilinear sample at texel centres, clamped to the edges.
    float sample(float u, float v) const {
        if (empty()) return 1.0f;
        const float fx = glm::clamp(u * width - 0.5f, 0.0f, (float)(width - 1));
        const float fy = glm::clamp(v * height - 0.5f, 0.0f, (float)(height - 1));
        const int x0 = (int)fx;
        const int y0 = (int)fy;
        const int x1 = std::min(x0 + 1, width - 1);
        const int y1 = std::min(y0 + 1, height - 1);
        const float tx = fx - x0;
        const float ty = fy - y0;
        const float top = glm::mix(values[y0 * width + x0], values[y0 * width + x1], tx);
        const float bottom = glm::mix(values[y1 * width + x0], values[y1 * width + x1], tx);
        return glm::mix(top, bottom, ty);
    }
};

// Everything one ILDA Mask node contributes. Several shapes are combined as a union
// (max); the texture grid multiplies the result; then invert and amount apply.
struct Field {
    Shape shape = Shape::None;
    std::vector<ShapeParams> shapes;
    Grid grid;
    bool useGrid = false;
    bool invert = false;
    float amount = 1.0f;
    std::vector<glm::vec2> rotations; // (cos, sin) per shape; call prepare() after changing shapes

    void prepare() {
        rotations.resize(shapes.size());
        for (size_t i = 0; i < shapes.size(); i++) rotations[i] = rotationOf(shapes[i]);
    }

    bool isIdentity() const {
        return amount <= 0.0f || (shape == Shape::None && !useGrid && !invert);
    }

    float sample(float u, float v) const {
        float m = 1.0f;
        if (shape != Shape::None) {
            m = 0.0f;
            for (size_t i = 0; i < shapes.size(); i++) {
                const glm::vec2 rotation = i < rotations.size() ? rotations[i] : rotationOf(shapes[i]);
                m = std::max(m, evaluateShape(shape, shapes[i], rotation, u, v));
            }
        }
        if (useGrid) m *= grid.sample(u, v);
        if (invert) m = 1.0f - m;
        m = glm::clamp(m, 0.0f, 1.0f);
        return 1.0f + (m - 1.0f) * glm::clamp(amount, 0.0f, 1.0f);
    }
};

// One output vertex of maskPolyline().
struct MaskedVertex {
    glm::vec3 position;
    float sourceIndex; // float index into the input vertices, for colour interpolation (wraps to 0 on the closing segment)
    float mask;
};

namespace detail {
// Douglas-Peucker on a 1D profile: keeps the samples needed so that linear interpolation
// between kept samples stays within `tolerance` of every sample.
inline void simplifyProfile(const std::vector<float>& m, int first, int last, float tolerance, std::vector<int>& keep) {
    if (last - first < 2) return;
    int worst = -1;
    float worstError = tolerance;
    for (int k = first + 1; k < last; k++) {
        const float t = float(k - first) / float(last - first);
        const float error = std::abs(m[k] - glm::mix(m[first], m[last], t));
        if (error > worstError) {
            worstError = error;
            worst = k;
        }
    }
    if (worst < 0) return;
    simplifyProfile(m, first, worst, tolerance, keep);
    keep.push_back(worst);
    simplifyProfile(m, worst, last, tolerance, keep);
}
}

// Samples the mask along every segment every `spacing` (canvas units) and inserts the
// vertices needed for linear colour interpolation to follow the mask within `tolerance`.
// Where the mask is flat no vertex is added. `canvasSize` converts vertex positions to u, v.
template<class Sampler>
void maskPolyline(const std::vector<glm::vec3>& vertices, bool closed, float canvasSize,
                  const Sampler& sampleMask, float spacing, float tolerance,
                  std::vector<MaskedVertex>& out, int maxSamplesPerSegment = 2048) {
    out.clear();
    const int n = (int)vertices.size();
    if (n == 0) return;

    auto sampleAt = [&](const glm::vec3& p) {
        return glm::clamp((float)sampleMask(p.x / canvasSize, p.y / canvasSize), 0.0f, 1.0f);
    };

    std::vector<float> profile;
    std::vector<int> keep;
    const int segments = closed && n > 1 ? n : n - 1;
    float startMask = sampleAt(vertices[0]);

    for (int i = 0; i < n; i++) {
        out.push_back({vertices[i], (float)i, startMask});
        if (i >= segments) break;

        const glm::vec3& a = vertices[i];
        const glm::vec3& b = vertices[(i + 1) % n];
        const float length = glm::length(glm::vec2(b - a)) / canvasSize;
        const int steps = std::min(std::max(1, (int)std::ceil(length / std::max(spacing, 1e-6f))), maxSamplesPerSegment);

        profile.resize(steps + 1);
        profile[0] = startMask;
        for (int k = 1; k <= steps; k++) {
            profile[k] = sampleAt(glm::mix(a, b, float(k) / steps));
        }

        keep.clear();
        detail::simplifyProfile(profile, 0, steps, tolerance, keep);
        for (int k : keep) {
            const float t = float(k) / steps;
            out.push_back({glm::mix(a, b, t), i + t, profile[k]});
        }
        startMask = profile[steps];
    }
}

}

#endif /* ildaMaskField_h */
