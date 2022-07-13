#include "client.hpp"
#include "connection.hpp"
#include "read_json.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <syslog.h>

using namespace std::chrono_literals;
const std::chrono::microseconds PERIOD = 1s;

static int generate(size_t idm) {
  static unsigned int seed = 777;
  float m = sqrt(rand_r(&seed) * idm + rand_r(&seed) * idm * idm +
                 rand_r(&seed) * idm * idm * idm + idm);
  return roundf(m);
}

client::~client() { close(MServerFd); }

bool client::read_config(const std::string &str) {
  MConfig = parse_file(str.c_str());
  assert(!MConfig.is_null() && "Config is null!");
  // std::stringstream ss;
  // pretty_print(ss, MConfig);
  // syslog(LOG_DEBUG, "%s", ss.str().c_str());
  return true;
}

void client::connect_to_server() {
  auto cfg = MConfig.get_object();
  std::string server_ip = json::serialize(cfg["ip_server"].get_string());
  uint64_t server_port = cfg["port_server"].get_int64();
  int num_of_metrics = cfg["number_of_metrics"].get_int64();

  struct sockaddr_in server_addr = {0};
  set_sockaddr(&server_addr, server_port);
  MServerFd = Socket(AF_INET, SOCK_STREAM, 0);
  Connect(MServerFd, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

json::value client::collect_metrics_from_sensors() {
  auto cfg = MConfig.get_object();
  const int num_of_metrics = cfg["number_of_metrics"].get_int64();
  const int rate_of_metrics = cfg["rate_of_metrics"].get_int64();
  const auto &metrics = cfg["mask_of_metrics"].get_array();

  json::array value_to_send;
  value_to_send.reserve(metrics.size());

  for (const auto &elem : metrics) {
    int number = elem.get_int64();
    assert(number < num_of_metrics);

    json::array data;
    data.reserve(rate_of_metrics);
    for (int j = 0; j < rate_of_metrics; ++j) {
      auto val = generate(number);
      data.push_back(val);
    }

    json::object new_obj;
    new_obj["_id"] = number;
    new_obj["data"] = data;
    value_to_send.push_back(new_obj);
  }
  return value_to_send;
}

void client::send_to_server(const json::value &value_to_send) {
  static uint64_t sent_msgs_count = 0;
  syslog(LOG_DEBUG, "Try to send Msg#: %lu", sent_msgs_count);
  std::stringstream ss;
  pretty_print(ss, value_to_send);
  const auto &str = ss.str();
  syslog(LOG_DEBUG, "Sending message: %s", str.c_str());
  write(MServerFd, str.c_str(), str.length());
  syslog(LOG_DEBUG,
         "the message has been sent!"
         " total sent msgs: %lu",
         ++sent_msgs_count);
}

std::string client::receive_from_server(int expected_metric_count) {
  static uint64_t rec_msgs_count = 0;
  syslog(LOG_DEBUG, "Try to receive Msg#: %lu", rec_msgs_count);
  std::stringstream ss;
  char buf[1024];
  while (true) {
    int nbytes = read(MServerFd, buf, sizeof(buf));
    if (nbytes == -1) {
      perror("read() failed");
      exit(EXIT_FAILURE);
    } else if (nbytes == 0) {
      close(MServerFd);
      break;
    } else {
      buf[nbytes] = '\0';
      ss << buf;

      auto recdata = parse_string(ss.str());
      if (recdata.is_array() &&
          recdata.get_array().size() == expected_metric_count) {
        break;
      }
    }
  }

  syslog(LOG_DEBUG, "Client received: \"%s\"", ss.str().c_str());
  syslog(LOG_DEBUG, "Total received msgs: %lu", ++rec_msgs_count);
  if (ss.str().empty()) {
    syslog(LOG_ERR, "Client exits because it took wrong/null data!");
    exit(EXIT_FAILURE);
  }
  return ss.str();
}

void client::save_data_to_file(const json::value &data) {
  auto cfg = MConfig.get_object();
  const auto expected_size = cfg["mask_of_metrics"].get_array().size();
  auto response = data.get_array();
  assert(expected_size == response.size());
  std::string log_dir =
      json::serialize(cfg["path_to_folder_of_log"].get_string());
  log_dir.erase(std::remove(log_dir.begin(), log_dir.end(), '\"'),
                log_dir.end());
  pid_t pid = getpid();

  for (auto &elem : response) {
    int idm = elem.get_object()["_id"].get_int64();
    std::filesystem::path path(log_dir);
    {
      std::stringstream filename;
      filename << pid << "_" << idm << "_"
               << "result"
               << ".txt";
      path /= filename.str();
    }

    // Note: open for overwriting, to add to the end change to std::ios::app
    std::ofstream ofile(path.c_str(), std::ios::out);
    if (ofile.is_open()) {
      syslog(LOG_DEBUG, "Save data to file: %s", path.c_str());
      std::stringstream ss;
      pretty_print(ss, elem.get_object()["result"]);
      ofile << ss.str() << std::endl;
    } else {
      // std::cerr << "File openning failed: " << path.c_str() << std::endl;
      syslog(LOG_ERR, "File openning failed: %s", path.c_str());
    }
  }
}

bool client::run() {

  connect_to_server();

  using ms = std::chrono::microseconds;
  auto prev_time = std::chrono::high_resolution_clock::now();

  auto adjust_sending_time = [&]() {
    auto new_time = std::chrono::high_resolution_clock::now();
    int left_usecs =
        std::chrono::duration_cast<ms>(PERIOD - (new_time - prev_time)).count();
    prev_time = new_time;

    syslog(LOG_DEBUG, "left_usec for sleep: %d", left_usecs);
    if (left_usecs > 0)
      usleep(left_usecs);
  };

  // main client loop
  while (true) {
    const auto &data_to_send = collect_metrics_from_sensors();

    // Client collects metrics and sends data every second
    adjust_sending_time();
    // It sleeps exactly 1 second, regardless of how long data were collected
    // usleep(std::chrono::duration_cast<ms>(PERIOD).count());

    send_to_server(data_to_send);

    int expected_metric_count = data_to_send.get_array().size();

    const auto &rdata_str = receive_from_server(expected_metric_count);

    if (MNeedSaveData) {
      const auto &rdata = parse_string(rdata_str);
      save_data_to_file(rdata);
    }
  }
}
