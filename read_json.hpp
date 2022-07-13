#ifndef __READ_JSON_HPP__
#define __READ_JSON_HPP__

#include <boost/json.hpp>
#include <sstream>

namespace json = boost::json;

json::value parse_string(const std::string &str);

json::value parse_file(char const *filename);

void pretty_print(std::stringstream &os, json::value const &jv,
                  std::string *indent = nullptr);

#endif /* __READ_JSON_HPP__ */
