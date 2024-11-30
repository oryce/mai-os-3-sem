#include "column_split.h"

#include <format>

#include "posix_buf.h"

using std::chrono::high_resolution_clock;
using std::chrono::time_point;

namespace column_split {

struct pthread_args {
	const vector<vector<long>>& arrays;

	vector<long>& result;

	size_t start;
	size_t end;
};

void* pthread_func(void* argsPtr) {
	auto args = static_cast<const pthread_args*>(argsPtr);

	for (size_t i = args->start; i != args->end; ++i) {
		for (const vector<long>& array : args->arrays) {
			args->result[i] += array[i];
		}
	}

	pthread_exit(nullptr);
}

vector<long> sum(const vector<vector<long>>& arrays, size_t threads) {
	size_t length = arrays[0].size();

	// Clamp threads to column amount.
	threads = std::min(length, threads);

	vector<long> result(length, 0);
	vector<pthread_t> pthreads;
	vector<pthread_args> args;

	args.reserve(threads);

	time_point timeStart = high_resolution_clock::now();

	for (size_t i = 0; i != threads; ++i) {
		size_t start = i * length / threads;
		size_t end = (i + 1) * length / threads;

		args.emplace_back(arrays, result, start, end);

		pthread_t pthread;
		if (pthread_create(&pthread, nullptr, &pthread_func, &args.back())) {
			throw std::runtime_error("can't create thread");
		}

		pthreads.push_back(pthread);
	}

	for (pthread_t& thread : pthreads) {
		pthread_join(thread, nullptr);
	}

	time_point timeEnd = high_resolution_clock::now();

	auto ns = (timeEnd - timeStart).count();
	auto ms = static_cast<double>(ns) / 1000000.0;
	pout << std::format("Took {}ns ({}ms)\n", ns, ms);

	return result;
}

}  // namespace column_split