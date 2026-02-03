#pragma once

#include <map>
#include <optional>
#include <string>

namespace cc::neolux::utils::unitransmit {

struct Options {
    bool blocking = true;
    int timeout_ms = -1;
    int max_connect = 1;
    std::string remote;
    bool broadcast = false;
    int baud = 115200;
    int data_bits = 8;
    int stop_bits = 1;
    char parity = 'n';
};

struct UrlParts {
    std::string scheme;
    std::string host;
    int port = -1;
    std::string path;
    std::map<std::string, std::string> query;
};

UrlParts parse_ifname(const std::string &ifname);
Options parse_options(const UrlParts &parts);
std::optional<std::pair<std::string, int>> parse_endpoint(const std::string &value);

} // namespace cc::neolux::utils::unitransmit
