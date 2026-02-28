#include "vertices.h"

void spheref(std::vector<float>& vertices, std::vector<unsigned int>& indices, int latitude, int longitude) {
    float r1 = 0.5f;
    for (int i = 0; i <= latitude; i++) {
        float beta = M_PI/2 - i * M_PI / latitude;
        float y = r1 * sin(beta);
        float r2 = r1 * cos(beta);
        for (int j = 0; j <= longitude; j++) {
            float alpha = j * 2 * M_PI / longitude;
            float x = r2 * cos(alpha);
            float z = r2 * sin(alpha);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    for (int i = 0; i < latitude; i++) {
        int k1 = i * (longitude + 1);
        int k2 = k1 + longitude + 1;
        for (int j = 0; j < longitude; j++, k1++, k2++) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1+1);
            }
            if (i != (latitude-1)) { 
                indices.push_back(k1+1);
                indices.push_back(k2);
                indices.push_back(k2+1);
            }
        }
    }
}

void gridf(std::vector<float>& grid, int size, float step)
{
    float half = size * step;

    for(int i = -size; i <= size; i++)
    {
        float k = i * step;

        grid.push_back(k);
        grid.push_back(0.0f);
        grid.push_back(-half);

        grid.push_back(k);
        grid.push_back(0.0f);
        grid.push_back(half);

        grid.push_back(-half);
        grid.push_back(0.0f);
        grid.push_back(k);

        grid.push_back(half);
        grid.push_back(0.0f);
        grid.push_back(k);
    }
}