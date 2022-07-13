#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <boost/json.hpp>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace json = boost::json;
using Config = json::value;

class server {
public:
  server() = default;

  ~server();

  bool read_config(const std::string &str);

  inline void set_need_save_data(bool val) { MNeedSaveData = val; }

  bool run();

private:
  void start_listening();

  std::string receive_from_client(int client_fd, int epollfd);

  json::value calc_confidence_score(int idm, const std::deque<int> &deque);

  json::value handle_data(const json::value &rdata);

  void send_to_client(int client_fd, const json::value &value_to_send);

  std::vector<int> calculate_fft(const std::vector<int> &AVal);

  void save_data_to_file(int idm, const json::value &data);

  int MListenSock;
  Config MConfig;
  bool MNeedSaveData = false;
  std::unordered_map<int, std::deque<int>> MMetricBuffer;
  std::unordered_map<std::string, int> MFilename2FdMap;
};

#endif /* __SERVER_HPP__ */
