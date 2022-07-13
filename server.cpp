#include "server.hpp"
#include "connection.hpp"
#include "read_json.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/epoll.h>

const int MAX_EVENTS = 64;
const int MAX_NUM_CLIENTS = 10'000;
const int MAX_NUM_METRICS = 1'000'000;

server::~server() {
  close(MListenSock);
  for (auto &fd : MFilename2FdMap) {
    close(fd.second);
  }
}

bool server::read_config(const std::string &str) {
  MConfig = parse_file(str.c_str());
  assert(!MConfig.is_null() && "Config is null!");
  // std::stringstream ss;
  // pretty_print(ss, MConfig);
  // std::cout << ss.str()  << std::endl;
  return true;
}

bool server::run() {

  start_listening();

  set_nonblocking(STDOUT_FILENO);

  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("epoll_create1 failed");
    exit(EXIT_FAILURE);
  }

  epoll_ctl_add(epollfd, MListenSock, EPOLLIN);

  struct epoll_event events[MAX_EVENTS];
  for (;;) {
    int event_count = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (event_count == -1) {
      perror("epoll_wait() failed");
      exit(EXIT_FAILURE);
    } else if (event_count == 0) {
      // return from epol_wait by timeout
      continue;
    }

    for (int i = 0; i < event_count; ++i) {
      if (events[i].data.fd == MListenSock) {
        while (true) {
          struct sockaddr_in client_addr;
          socklen_t client_addr_len = sizeof(client_addr);
          int client_fd = accept(MListenSock, (struct sockaddr *)&client_addr,
                                 &client_addr_len);
          if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // we processed all of the connections
              break;
            } else {
              perror("accept() failed");
              exit(EXIT_FAILURE);
            }
          } else {
            set_nonblocking(client_fd);
            epoll_ctl_add(epollfd, client_fd, EPOLLIN);
            break;
          }
        }
      } else {
        if (events[i].events & EPOLLIN) {
          int client_fd = events[i].data.fd;

          const auto &rdata_str = receive_from_client(client_fd, epollfd);
          if (rdata_str.empty())
            continue;

          const auto &rdata = parse_string(rdata_str);

          const auto &response_to_send = handle_data(rdata);

          send_to_client(client_fd, response_to_send);
        } else {
          std::cerr << "Unexpected case while handling event" << std::endl;
          exit(EXIT_FAILURE);
        }
      }

      if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
        close(events[i].data.fd);
        continue;
      }
    }
  }

  if (close(epollfd)) {
    perror("close() epollfd failed");
    exit(EXIT_FAILURE);
  }
}

void server::start_listening() {
  auto cfg = MConfig.get_object();
  std::string listen_ip = json::serialize(cfg["listen_ip"].get_string());
  uint64_t listen_port = cfg["listen_port"].get_int64();

  struct sockaddr_in server_addr = {0};
  set_sockaddr(&server_addr, listen_port);
  MListenSock = Socket(AF_INET, SOCK_STREAM, 0);

  int enable = 1;
  if (setsockopt(MListenSock, SOL_SOCKET, SO_REUSEADDR, &enable,
                 sizeof(enable)) == -1)
    perror("setsockopt failed");

  Bind(MListenSock, (struct sockaddr *)&server_addr, sizeof(server_addr));

  Listen(MListenSock, MAX_NUM_CLIENTS);

  set_nonblocking(MListenSock);
}

std::string server::receive_from_client(int client_fd, int epollfd) {
  static uint64_t rec_msgs_count = 0;
  // std::cout << "Try to receive Msg#: " << rec_msgs_count << std::endl;
  std::stringstream ss;
  char buf[1024];
  while (true) {
    int nbytes = read(client_fd, buf, sizeof(buf));
    if (nbytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // std::cout << "Finished reading data from client" << std::endl;
        break;
      } else {
        perror("read() failed");
        exit(EXIT_FAILURE);
      }
    } else if (nbytes == 0) {
      close(client_fd);
      epoll_ctl(epollfd, EPOLL_CTL_DEL, client_fd, NULL);
      break;
    } else {
      buf[nbytes] = '\0';
      ss << buf;
    }
  }
  // std::cout << "Server received: \"" << ss.str() << "\"" << std::endl;
  // std::cout << "Total received msgs: " << ++rec_msgs_count << std::endl;
  return ss.str();
}

json::value server::calc_confidence_score(int idm,
                                          const std::deque<int> &deque) {
  auto do_average = [](std::deque<int> const &v) -> double {
    if (v.empty())
      return 0.0;
    double partial_sum = 0.0;
    std::for_each(std::begin(v), std::end(v),
                  [&](const int d) { partial_sum += d / ((double)v.size()); });
    return partial_sum;
  };

  auto do_dispersion = [](std::deque<int> const &v, double m) -> double {
    if (v.empty())
      return 0.0;
    double partial_sum = 0.0;
    std::for_each(std::begin(v), std::end(v), [&](const int d) {
      partial_sum += (d - m) * (d - m) / ((double)v.size());
    });
    return partial_sum;
  };

  auto round_2d = [](double value) { return round(value * 100.0) / 100.0; };

  double average = do_average(deque);
  double dispersion = do_dispersion(deque, average);
  double standard_deviation = sqrt(dispersion);
  // TODO: find difference between them, Note: for now, they remain equel!
  double sq_standard_deviation = standard_deviation;

  json::object new_obj;
  new_obj["_id"] = idm;

  json::object r;
  r["average"].emplace_double() = round_2d(average);
  r["sq_standard_deviation"].emplace_double() = round_2d(sq_standard_deviation);
  r["standard_deviation"].emplace_double() = round_2d(standard_deviation);
  r["dispersion"].emplace_double() = round_2d(dispersion);
  new_obj["result"] = r;
  return new_obj;
}

json::value server::handle_data(const json::value &rdata) {
  // To get the nearest number which is a power of two
  auto nearest_power_of_2 = [](size_t x) {
    return 1 << (long)(log(x) / log(2));
  };

  json::array response;
  for (auto elem : rdata.get_array()) {
    // 1. Server aggregates received data
    auto obj = elem.get_object();
    int idm = obj["_id"].get_int64();
    auto &deque = MMetricBuffer[idm];
    auto newdata = obj["data"].get_array();

    int extra_elems = (deque.size() + newdata.size()) - MAX_NUM_METRICS;
    // deque.insert(deque.end(), newdata.begin(), newdata.end());
    for (auto e : newdata)
      deque.push_back(e.get_int64());

    if (extra_elems > 0) {
      deque.erase(deque.begin(), deque.begin() + extra_elems);
    }
    // std::cout << "buffer[idm=" << idm << "] size: " << deque.size() <<
    // std::endl;

    // 2. Server calculates the confidence score of the data
    auto metric_score = calc_confidence_score(idm, deque);
    response.push_back(metric_score);

    // 3. Server executes FFT
    size_t n = nearest_power_of_2(deque.size());
    // get last n elements from buffer
    std::vector<int> arr(deque.end() - n, deque.end());
    auto start_time = std::chrono::high_resolution_clock::now();
    auto spectrum_result = calculate_fft(arr);
    auto end_time = std::chrono::high_resolution_clock::now();
    typedef std::chrono::milliseconds ms;
    size_t spent_ms =
        std::chrono::duration_cast<ms>(end_time - start_time).count();

    {
      // 4. Server prints calculation results
      auto res = metric_score.get_object()["result"].get_object();
      double average = res["average"].as_double();
      double sq_standard_deviation = res["sq_standard_deviation"].as_double();
      double standard_deviation = res["standard_deviation"].as_double();
      double dispersion = res["dispersion"].as_double();

      const std::string sep = "; ";
      std::stringstream ss;
      ss << idm << sep << newdata.size() << sep << deque.size() << sep
         << average << sep << sq_standard_deviation << sep << standard_deviation
         << sep << dispersion << sep << spent_ms << std::endl;
      write(STDOUT_FILENO, ss.str().c_str(), ss.str().length());
    }

    if (MNeedSaveData) {
      // 5. Server saves the spectrum to file
      json::array rdata(spectrum_result.begin(), spectrum_result.end());
      save_data_to_file(idm, rdata);
    }
  }
  return response;
}

void server::send_to_client(int client_fd, const json::value &value_to_send) {
  static uint64_t sent_msgs_count = 0;
  // std::cout << "Try to send Msg#: " << sent_msgs_count << std::endl;
  std::stringstream ss;
  pretty_print(ss, value_to_send);
  const auto &str = ss.str();
  // std::cout << "Sending message: " << str << std::endl;
  write(client_fd, str.c_str(), str.length());
  // std::cout << "the message has been sent!"
  //              " total sent msgs:" << ++sent_msgs_count << std::endl;
}

void server::save_data_to_file(int idm, const json::value &data) {
  auto cfg = MConfig.get_object();
  auto log_dir = json::serialize(cfg["path_to_folder_of_log"].get_string());
  log_dir.erase(std::remove(log_dir.begin(), log_dir.end(), '\"'),
                log_dir.end());

  std::filesystem::path path(log_dir);
  std::stringstream filename;
  filename << idm << "_"
           << "spectrum"
           << ".txt";
  path /= filename.str();

  int fd = -1;
  auto res = MFilename2FdMap.find(path.native());
  if (res != MFilename2FdMap.end()) {
    fd = res->second;
  } else {
    fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC | O_NONBLOCK,
              S_IRUSR | S_IWUSR);
    if (fd == -1) {
      perror("open() failed");
      exit(EXIT_FAILURE);
    }
    MFilename2FdMap[path.native()] = fd;
  }
  // std::cout << "Save data to file: " << path.c_str() << std::endl;
  std::stringstream ss;
  pretty_print(ss, data);
  pwrite(fd, ss.str().c_str(), ss.str().size(), 0);
  // close(fd);
}

static void FFT_analysis(const std::vector<int> &AVal, std::vector<int> &FTvl,
                         int Nvl, int Nft) {
  // https://ru.wikibooks.org/wiki/Реализации_алгоритмов/Быстрое_преобразование_Фурье
  const double TwoPi = 6.283185307179586;
  int i, j, n, m, Mmax, Istp;
  double Tmpr, Tmpi, Wtmp, Theta;
  double Wpr, Wpi, Wr, Wi;
  double *Tmvl;

  n = Nvl * 2;
  Tmvl = new double[n];

  for (i = 0; i < n; i += 2) {
    Tmvl[i] = 0;
    Tmvl[i + 1] = AVal[i / 2];
  }

  i = 1;
  j = 1;
  while (i < n) {
    if (j > i) {
      Tmpr = Tmvl[i];
      Tmvl[i] = Tmvl[j];
      Tmvl[j] = Tmpr;
      Tmpr = Tmvl[i + 1];
      Tmvl[i + 1] = Tmvl[j + 1];
      Tmvl[j + 1] = Tmpr;
    }
    i = i + 2;
    m = Nvl;
    while ((m >= 2) && (j > m)) {
      j = j - m;
      m = m >> 1;
    }
    j = j + m;
  }

  Mmax = 2;
  while (n > Mmax) {
    Theta = -TwoPi / Mmax;
    Wpi = sin(Theta);
    Wtmp = sin(Theta / 2);
    Wpr = Wtmp * Wtmp * 2;
    Istp = Mmax * 2;
    Wr = 1;
    Wi = 0;
    m = 1;

    while (m < Mmax) {
      i = m;
      m = m + 2;
      Tmpr = Wr;
      Tmpi = Wi;
      Wr = Wr - Tmpr * Wpr - Tmpi * Wpi;
      Wi = Wi + Tmpr * Wpi - Tmpi * Wpr;

      while (i < n) {
        j = i + Mmax;
        Tmpr = Wr * Tmvl[j] - Wi * Tmvl[j - 1];
        Tmpi = Wi * Tmvl[j] + Wr * Tmvl[j - 1];

        Tmvl[j] = Tmvl[i] - Tmpr;
        Tmvl[j - 1] = Tmvl[i - 1] - Tmpi;
        Tmvl[i] = Tmvl[i] + Tmpr;
        Tmvl[i - 1] = Tmvl[i - 1] + Tmpi;
        i = i + Istp;
      }
    }

    Mmax = Istp;
  }

  for (i = 0; i < Nft; i++) {
    j = i * 2;
    FTvl[i] = 2 * sqrt(pow(Tmvl[j], 2) + pow(Tmvl[j + 1], 2)) / Nvl;
  }
  delete[] Tmvl;
}

std::vector<int> server::calculate_fft(const std::vector<int> &AVal) {
  std::vector<int> FTvl(AVal.size(), 0);

  auto is_power_of_two = [](int v) -> bool { return v && !(v & (v - 1)); };
  if (is_power_of_two(AVal.size()))
    FFT_analysis(AVal, FTvl, AVal.size(), AVal.size());

  // std::cout << "calculate_fft for size: " << FTvl.size() << std::endl;
  // for (auto e: FTvl) std::cout << e << ",";
  // std::cout << std::endl;
  return FTvl;
}
