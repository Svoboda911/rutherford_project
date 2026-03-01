#include "geometry.h"

std::vector<float> geometry::circle(int segments, float radius) {
    std::vector<float> vertices = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }
    return vertices;
}
