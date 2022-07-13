#ifndef __CLIENT_HPP__
#define __CLIENT_HPP__

#include <boost/json.hpp>

namespace json = boost::json;
using Config = json::value;

class client {
public:
  client() = default;

  ~client();

  bool read_config(const std::string &str);

  inline void set_need_save_data(bool val) { MNeedSaveData = val; }

  bool run();

private:
  void connect_to_server();

  json::value collect_metrics_from_sensors();

  void send_to_server(const json::value &value_to_send);

  std::string receive_from_server(int expected_metric_count);

  void save_data_to_file(const json::value &data);

  int MServerFd;
  Config MConfig;
  bool MNeedSaveData = false;
};

#endif /* __CLIENT_HPP__ */
