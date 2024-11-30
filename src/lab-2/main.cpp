#include <format>
#include <sstream>
#include <string>
#include <vector>

#include "column_split.h"
#include "posix_buf.h"
#include "row_split.h"

using std::vector;

int main(int argc, char* argv[]) {
	if (argc != 3) {
		perr << std::format(
		    "usage: {} <k> <threads>\n"
		    "\n"
		    "Sums array columns using multiple threads.\n"
		    "\n"
		    "  <k>       -- number of arrays\n"
		    "  <threads> -- number of threads\n",
		    argv[0]);
		return 1;
	}

	size_t k;
	{
		std::istringstream stream(argv[1]);
		if (!(stream >> k)) {
			perr << "Invalid `k`; malformed number\n";
			return 1;
		}
	}

	size_t threads;
	{
		std::istringstream stream(argv[2]);
		if (!(stream >> threads)) {
			perr << "Invalid `threads`; malformed number\n";
			return 1;
		}
	}

	pout << "Input `k` number arrays with the same lengths; "
	        "one array per line, numbers are separated with spaces"
	     << std::endl;

	vector<vector<long>> arrays(k);

	for (size_t i = 0; i != k; ++i) {
		std::string line;
		std::getline(pin, line);

		std::istringstream stream(line);
		long number;

		while (stream >> number) {
			arrays[i].push_back(number);
		}
	}

	for (size_t i = 1; i != k; ++i) {
		if (arrays[i].size() != arrays[i - 1].size()) {
			perr << "Arrays have different lengths :/ "
			        "Perhaps you supplied an incorrect `k`"
			     << std::endl;
			return 1;
		}
	}

	vector<long> result;

	double rowsToColumns =
	    static_cast<double>(arrays.size()) / static_cast<double>(arrays[0].size());
	pout << "row / column ratio: " << rowsToColumns << std::endl;

	if (rowsToColumns > 2.0) {
		pout << "  ==> using row_split" << std::endl;
		result = row_split::sum(arrays, threads);
	} else {
		pout << "  ==> using column_split" << std::endl;
		result = column_split::sum(arrays, threads);
	}

	for (long item : result) {
		pout << item << " ";
	}

	return 0;
}