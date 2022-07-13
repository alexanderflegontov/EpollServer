#include "read_json.hpp"
#include <boost/json.hpp>
#include <fstream>
#include <iostream>
#include <string>

namespace json = boost::json;

json::value parse_string(const std::string &str) {
  json::stream_parser p;
  json::error_code ec;

  p.write(str.c_str(), str.size(), ec);

  if (ec)
    return nullptr;
  p.finish(ec);
  if (ec)
    return nullptr;
  return p.release();
}

json::value parse_file(char const *filename) {
  json::stream_parser p;
  json::error_code ec;

  std::fstream ifile(filename, std::ios::in);
  if (ifile.is_open()) {
    std::string line;
    while (ifile >> line) {
      // std::cout << line << ' ';
      p.write(line.c_str(), line.size(), ec);
    }
  }

  if (ec)
    return nullptr;
  p.finish(ec);
  if (ec)
    return nullptr;
  return p.release();
}

void pretty_print(std::stringstream &os, json::value const &jv,
                  std::string *indent) {
  std::string indent_;
  if (!indent)
    indent = &indent_;
  switch (jv.kind()) {
  case json::kind::object: {
    os << "{\n";
    indent->append(4, ' ');
    auto const &obj = jv.get_object();
    if (!obj.empty()) {
      auto it = obj.begin();
      for (;;) {
        os << *indent << json::serialize(it->key()) << " : ";
        pretty_print(os, it->value(), indent);
        if (++it == obj.end())
          break;
        os << ",\n";
      }
    }
    os << "\n";
    indent->resize(indent->size() - 4);
    os << *indent << "}";
    break;
  }

  case json::kind::array: {
    os << "[\n";
    indent->append(4, ' ');
    auto const &arr = jv.get_array();
    if (!arr.empty()) {
      auto it = arr.begin();
      for (;;) {
        os << *indent;
        pretty_print(os, *it, indent);
        if (++it == arr.end())
          break;
        os << ",\n";
      }
    }
    os << "\n";
    indent->resize(indent->size() - 4);
    os << *indent << "]";
    break;
  }

  case json::kind::string: {
    os << json::serialize(jv.get_string());
    break;
  }

  case json::kind::uint64:
    os << jv.get_uint64();
    break;

  case json::kind::int64:
    os << jv.get_int64();
    break;

  case json::kind::double_:
    os << jv.get_double();
    break;

  case json::kind::bool_:
    if (jv.get_bool())
      os << "true";
    else
      os << "false";
    break;

  case json::kind::null:
    os << "null";
    break;
  }

  if (indent->empty())
    os << "\n";
}
