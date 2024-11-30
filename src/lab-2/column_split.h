#pragma once

#include <vector>

using std::vector;

namespace column_split {

vector<long> sum(const vector<vector<long>>& arrays, size_t threads);

}