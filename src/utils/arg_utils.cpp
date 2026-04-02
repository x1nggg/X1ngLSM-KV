#include "x1nglsm/utils/arg_utils.hpp"
#include "x1nglsm/cli/commands.hpp"

#include <iostream>
#include <string>

namespace x1nglsm::utils {

void parse_args(int argc, char *argv[], std::string &out_dir) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      cli::print_args_help();
      exit(0);
    } else if (arg == "--dir") {
      if (i + 1 < argc) {
        out_dir = argv[++i];
      } else {
        std::cerr << "Error: --dir requires an argument\n";
        exit(1);
      }
    } else {
      std::cerr << "Error: Unknown argument " << arg << "\n";
      std::cerr << "Use --help or -h to see available options.\n";
      exit(1);
    }
  }
}

} // namespace x1nglsm::utils