#include "client.hpp"
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>

const char *DAEMON_NAME = "client_daemon";
int daemonize = 1;
bool need_save_data = false;
std::string path_to_config = "./configs/client.cfg";

void prepare_daemon(int daemonize) {
  auto signal_handler = [](int sig) {
    switch (sig) {
    case SIGHUP:
      syslog(LOG_WARNING, "Received SIGHUP signal.");
      break;
    case SIGTERM:
      syslog(LOG_WARNING, "Received SIGTERM signal.");
      break;
    default:
      syslog(LOG_WARNING, "Unhandled signal (%d) %s", sig, strsignal(sig));
      break;
    }
  };

  signal(SIGHUP, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler);

  setlogmask(LOG_UPTO(LOG_INFO));
  // setlogmask(LOG_UPTO(LOG_DEBUG));
  openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
  syslog(LOG_INFO, "%s client starting up", DAEMON_NAME);

  if (daemonize) {
    syslog(LOG_INFO, "starting the client process");

    pid_t pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    } else if (pid > 0) {
      // Exit the parent process.
      exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Create a new SID for the child process
    pid_t sid = setsid();
    if (sid < 0) {
      perror("setsid() failed");
      exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
      perror("chdir() failed");
      exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }
}

void handle_args(int argc, char *argv[]) {
  auto print_usage = [&]() {
    if (argc >= 1) {
      std::cout << "Usage: " << argv[0] << "[OPTIONS]" << std::endl;
      std::cout << "\t"
                << "-h"
                << "  "
                << "Show this help screen." << std::endl;
      std::cout << "\t"
                << "-n"
                << "  "
                << "Don't fork off as a daemon." << std::endl;
      std::cout << "\t"
                << "-l"
                << "  "
                << "Switch ON logging data from server"
                   " to $PID_$IDM_result.txt. Default [off]"
                << std::endl;
      std::cout << "\t"
                << "-c"
                << "  "
                << "Path to config file used at startup." << std::endl;
      std::cout << std::endl;
    }
  };

  int c;
  while ((c = getopt(argc, argv, "nhlc:|help")) != -1) {
    switch (c) {
    case 'h':
      print_usage();
      exit(0);
      break;
    case 'n':
      daemonize = 0;
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

  prepare_daemon(daemonize);

  // Main logic of the client processing
  client c;
  c.read_config(path_to_config);
  c.set_need_save_data(need_save_data);
  c.run();

  syslog(LOG_INFO, "%s daemon exiting", DAEMON_NAME);
  return 0;
}
