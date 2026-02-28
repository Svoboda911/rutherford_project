#pragma once
#include <vector>
#include <cmath>

void spheref(std::vector<float>& vertices, std::vector<unsigned int>& indices, int latitude, int longitude);
void gridf(std::vector<float>& grid, int size, float step);