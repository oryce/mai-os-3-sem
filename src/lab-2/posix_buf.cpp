#include "posix_buf.h"

static posix_streambuf stdoutBuf(STDOUT_FILENO);
std::ostream pout(&stdoutBuf);

static posix_streambuf stdinBuf(STDIN_FILENO);
std::istream pin(&stdinBuf);

static posix_streambuf stderrBuf(STDERR_FILENO);
std::ostream perr(&stderrBuf);
