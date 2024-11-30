#include "row_split.h"

#include <format>

#include "posix_buf.h"

using std::chrono::high_resolution_clock;
using std::chrono::time_point;

namespace row_split {

struct pthread_args {
	const vector<vector<long>>& arrays;

	/** Owned vector with temporary result for the row.*/
	vector<long> result;

	size_t start;
	size_t end;
};

void* pthread_func(void* argsPtr) {
	auto args = static_cast<pthread_args*>(argsPtr);
	// Length is the same for all arrays.
	size_t length = args->arrays[0].size();

	for (size_t i = args->start; i != args->end; ++i) {
		for (size_t j = 0; j != length; ++j) {
			args->result[j] += args->arrays[i][j];
		}
	}

	pthread_exit(argsPtr);
}

vector<long> sum(const vector<vector<long>>& arrays, size_t threads) {
	size_t length = arrays[0].size();
	size_t height = arrays.size();

	// Clamp threads to row amount.
	threads = std::min(height, threads);

	vector<long> result(length, 0);
	vector<pthread_t> pthreads;
	vector<pthread_args> args;

	// Reserve capacity for all threads, so that the vector isn't reallocated.
	args.reserve(threads);

	time_point timeStart = high_resolution_clock::now();

	for (size_t i = 0; i != threads; ++i) {
		size_t start = i * height / threads;
		size_t end = (i + 1) * height / threads;

		// If there's nothing to sum, don't start the thread, it won't do any
		// calculations.
		if (end - start == 1) {
			for (size_t j = 0; j != length; ++j) {
				result[j] += arrays[start][j];
			}
			continue;
		}

		vector<long> tempResult(length, 0);
		args.emplace_back(arrays, std::move(tempResult), start, end);

		pthread_t pthread;
		if (pthread_create(&pthread, nullptr, &pthread_func, &args.back())) {
			throw std::runtime_error("can't create thread");
		}

		pthreads.push_back(pthread);
	}

	for (pthread_t& thread : pthreads) {
		pthread_args* threadResult;
		if (pthread_join(thread, reinterpret_cast<void**>(&threadResult))) {
			throw std::runtime_error("can't join thread");
		}

		for (size_t i = 0; i != length; ++i) {
			result[i] += threadResult->result[i];
		}
	}

	time_point timeEnd = high_resolution_clock::now();

	auto ns = (timeEnd - timeStart).count();
	auto ms = static_cast<double>(ns) / 1000000.0;
	pout << std::format("Took {}ns ({}ms)\n", ns, ms);

	return result;
}

}  // namespace row_split