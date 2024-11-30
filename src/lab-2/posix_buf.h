#pragma once

#include <unistd.h>

#include <istream>
#include <ostream>
#include <streambuf>

extern std::ostream pout;
extern std::istream pin;
extern std::ostream perr;

class posix_streambuf : public std::streambuf {
	static const size_t BUFFER_SIZE = 4096;

	int fd_;
	char in_buf_[BUFFER_SIZE];
	char out_buf_[BUFFER_SIZE];

	bool flush() {
		size_t length = pptr() - pbase();

		if (length) {
			ssize_t written = write(fd_, pbase(), length);
			if (written != length) return false;
		}

		setp(out_buf_, out_buf_ + BUFFER_SIZE);
		return true;
	}

   public:
	explicit posix_streambuf(int fd) : fd_(fd), in_buf_(), out_buf_() {
		setp(out_buf_, out_buf_ + BUFFER_SIZE);
		setg(in_buf_, in_buf_ + BUFFER_SIZE, in_buf_ + BUFFER_SIZE);
	}

	~posix_streambuf() { flush(); }

   protected:
	int sync() override { return flush() ? 0 : -1; }

	std::streamsize xsputn(const char *s, std::streamsize n) override {
		std::streamsize total = 0;

		while (n) {
			std::streamsize space = epptr() - pptr();
			if (!space) {
				if (!flush()) return 0;
				space = epptr() - pptr();
			}

			std::streamsize written = std::min(n, space);
			std::copy(s, s + written, pptr());
			pbump(static_cast<int>(written));

			n -= written;
			s += written;
			total += written;
		}

		return total;
	}

	int_type overflow(int_type c) override {
		if (c != EOF) {
			*pptr() = static_cast<char>(c);
			pbump(1);
		}

		if (!flush()) {
			return traits_type::eof();
		}

		return c;
	}

	std::streamsize xsgetn(char *s, std::streamsize n) override {
		std::streamsize total = 0;

		while (n) {
			std::streamsize space = egptr() - gptr();
			if (!space) {
				if (underflow() == traits_type::eof()) break;
				space = egptr() - gptr();
			}

			std::streamsize read = std::min(n, space);
			std::copy(s, s + read, gptr());
			gbump(static_cast<int>(read));

			s += read;
			n -= read;
			total += read;
		}

		return total;
	}

	int_type underflow() override {
		if (gptr() < egptr()) {
			// Still have some data in the buffer
			return traits_type::to_int_type(*gptr());
		}

		ssize_t nRead = read(fd_, in_buf_, BUFFER_SIZE);
		if (nRead <= 0) {
			return traits_type::eof();
		}

		setg(in_buf_, in_buf_, in_buf_ + nRead);
		return traits_type::to_int_type(*gptr());
	}
};