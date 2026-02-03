#include "cc/neolux/utils/unitransmit/Protocol.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace cc::neolux::utils::unitransmit {
namespace {

std::string trim_copy(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](unsigned char ch) { return !is_space(ch); })
                        .base(),
                value.end());
    return value;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::map<std::string, std::string> parse_query(const std::string &query) {
    std::map<std::string, std::string> result;
    std::size_t start = 0;
    while (start < query.size()) {
        auto end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }
        auto eq = query.find('=', start);
        if (eq != std::string::npos && eq < end) {
            auto key = query.substr(start, eq - start);
            auto value = query.substr(eq + 1, end - eq - 1);
            result[to_lower_copy(trim_copy(key))] = trim_copy(value);
        } else {
            auto key = query.substr(start, end - start);
            result[to_lower_copy(trim_copy(key))] = "";
        }
        start = end + 1;
    }
    return result;
}

} // namespace

UrlParts parse_ifname(const std::string &ifname) {
    UrlParts out;
    std::string trimmed = trim_copy(ifname);
    auto scheme_pos = trimmed.find("://");
    std::string rest;
    if (scheme_pos != std::string::npos) {
        out.scheme = trimmed.substr(0, scheme_pos);
        rest = trimmed.substr(scheme_pos + 3);
    } else {
        auto colon_pos = trimmed.find(':');
        auto slash_pos = trimmed.find('/');
        if (colon_pos != std::string::npos && (slash_pos == std::string::npos || colon_pos < slash_pos)) {
            out.scheme = trimmed.substr(0, colon_pos);
            rest = trimmed.substr(colon_pos + 1);
        } else if (slash_pos != std::string::npos) {
            out.scheme = trimmed.substr(0, slash_pos);
            rest = trimmed.substr(slash_pos + 1);
        } else {
            out.scheme = trimmed;
            rest = "";
        }
    }
    out.scheme = to_lower_copy(trim_copy(out.scheme));

    auto query_pos = rest.find('?');
    if (query_pos != std::string::npos) {
        out.query = parse_query(rest.substr(query_pos + 1));
        rest = rest.substr(0, query_pos);
    }

    if (out.scheme == "serial") {
        out.path = rest;
        if (out.path.empty()) {
            auto path_it = out.query.find("path");
            if (path_it != out.query.end()) {
                out.path = path_it->second;
            }
        }
    } else {
        auto slash_pos = rest.find('/');
        if (slash_pos != std::string::npos) {
            out.path = rest.substr(slash_pos + 1);
            rest = rest.substr(0, slash_pos);
        }
        auto colon_pos = rest.rfind(':');
        if (colon_pos != std::string::npos) {
            out.host = rest.substr(0, colon_pos);
            auto port_str = rest.substr(colon_pos + 1);
            if (!port_str.empty()) {
                out.port = std::atoi(port_str.c_str());
            }
        } else {
            out.host = rest;
        }
    }

    if (out.host.empty()) {
        auto host_it = out.query.find("host");
        if (host_it != out.query.end()) {
            out.host = host_it->second;
        }
    }
    if (out.port <= 0) {
        auto port_it = out.query.find("port");
        if (port_it != out.query.end()) {
            out.port = std::atoi(port_it->second.c_str());
        }
    }
    return out;
}

Options parse_options(const UrlParts &parts) {
    Options opts;
    auto it = parts.query.find("blocking");
    if (it != parts.query.end()) {
        opts.blocking = (it->second != "0" && it->second != "false");
    }
    it = parts.query.find("timeout_ms");
    if (it != parts.query.end()) {
        opts.timeout_ms = std::atoi(it->second.c_str());
    }
    it = parts.query.find("max_connect");
    if (it != parts.query.end()) {
        int value = std::atoi(it->second.c_str());
        if (value > 0) {
            opts.max_connect = value;
        }
    }
    it = parts.query.find("remote");
    if (it != parts.query.end()) {
        opts.remote = it->second;
    }
    it = parts.query.find("broadcast");
    if (it != parts.query.end()) {
        opts.broadcast = (it->second == "1" || it->second == "true");
    }
    it = parts.query.find("baud");
    if (it != parts.query.end()) {
        opts.baud = std::atoi(it->second.c_str());
    }
    it = parts.query.find("data");
    if (it != parts.query.end()) {
        opts.data_bits = std::atoi(it->second.c_str());
    }
    it = parts.query.find("stop");
    if (it != parts.query.end()) {
        opts.stop_bits = std::atoi(it->second.c_str());
    }
    it = parts.query.find("parity");
    if (it != parts.query.end() && !it->second.empty()) {
        opts.parity = static_cast<char>(std::tolower(it->second[0]));
    }
    return opts;
}

std::optional<std::pair<std::string, int>> parse_endpoint(const std::string &value) {
    auto colon_pos = value.rfind(':');
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }
    auto host = value.substr(0, colon_pos);
    auto port_str = value.substr(colon_pos + 1);
    if (port_str.empty()) {
        return std::nullopt;
    }
    int port = std::atoi(port_str.c_str());
    if (port <= 0) {
        return std::nullopt;
    }
    return std::make_pair(host, port);
}

} // namespace cc::neolux::utils::unitransmit
