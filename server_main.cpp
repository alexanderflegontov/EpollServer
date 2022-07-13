#include "server.hpp"
#include <iostream>

bool need_save_data = false;
std::string path_to_config = "./configs/server.cfg";

void handle_args(int argc, char *argv[]) {
  auto print_usage = [&]() {
    if (argc >= 1) {
      std::cout << "Usage: " << argv[0] << "[OPTIONS]" << std::endl;
      std::cout << "\t"
                << "-h"
                << "  "
                << "Show this help screen." << std::endl;
      std::cout << "\t"
                << "-l"
                << "  "
                << "Switch ON server logger"
                   " to $IDM_spectrum.txt. Default [off]"
                << std::endl;
      std::cout << "\t"
                << "-c"
                << "  "
                << "Path to config file used at startup." << std::endl;
      std::cout << std::endl;
    }
  };

  int c;
  while ((c = getopt(argc, argv, "hlc:|help")) != -1) {
    switch (c) {
    case 'h':
      print_usage();
      exit(0);
      break;
    case 'l':
      need_save_data = true;
      break;
    case 'c':
      path_to_config = optarg;
      // std::cout << "path_to_config = " << path_to_config << std::endl;
      break;
    default:
      print_usage();
      exit(0);
      break;
    }
  }
}

int main(int argc, char *argv[]) {

  handle_args(argc, argv);

  // Main logic of the server processing
  server s;
  s.read_config(path_to_config);
  s.set_need_save_data(need_save_data);
  s.run();

  return 0;
}
