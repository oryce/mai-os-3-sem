#pragma once

#include <vector>

using std::vector;

namespace row_split {

vector<long> sum(const vector<vector<long>>& arrays, size_t threads);

}