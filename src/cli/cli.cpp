#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // Windows Vista or later
#endif

#include "x1nglsm/cli/commands.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace x1nglsm::cli;

// ========== 主函数 ==========

int main(int argc, char *argv[]) { return run_cli(argc, argv); }