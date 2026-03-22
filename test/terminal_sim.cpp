#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using cc::neolux::utils::unitransmit::UniTransmit;

namespace {

void print_instance_state(const UniTransmit &instance) {
    std::cout << " ready=" << (instance.is_ready() ? 1 : 0);
    const auto error = instance.last_error();
    if (!error.empty()) {
        std::cout << " error=\"" << error << "\"";
    }
}

std::string trim_left(const std::string &value) {
    std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    return value.substr(start);
}

void print_bytes(const std::vector<std::uint8_t> &data) {
    std::cout << "size=" << data.size();
    if (!data.empty()) {
        std::cout << " str=\"";
        for (auto ch : data) {
            if (ch >= 32 && ch < 127) {
                std::cout << static_cast<char>(ch);
            } else {
                std::cout << '.';
            }
        }
        std::cout << "\" hex=";
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(data[i]);
        }
        std::cout << std::dec;
    }
    std::cout << "\n";
}

void print_help() {
    std::cout
        << "Commands:\n"
        << "  help\n"
        << "  list\n"
        << "  create <name> <ifname>\n"
        << "  delete <name>\n"
        << "  start <name>\n"
        << "  close <name>\n"
        << "  write <name> <text>\n"
        << "  read <name>\n"
        << "  readn <name> <n>\n"
        << "  readall <name>\n"
        << "  in_waiting <name>\n"
        << "  on_recv <name>\n"
        << "  off_recv <name>\n"
        << "  quit\n";
}

} // namespace

int main() {
    std::map<std::string, std::unique_ptr<UniTransmit>> instances;
    std::string line;

    std::cout << "UniTransmit terminal simulator. Type 'help' for commands.\n";
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) {
            continue;
        }

        if (cmd == "help") {
            print_help();
            continue;
        }
        if (cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "list") {
            if (instances.empty()) {
                std::cout << "(no instances)\n";
                continue;
            }
            for (const auto &pair : instances) {
                std::cout << pair.first << " (" << pair.second->ifname() << ")";
                print_instance_state(*pair.second);
                std::cout << "\n";
            }
            continue;
        }
        if (cmd == "create") {
            std::string name;
            iss >> name;
            std::string ifname;
            std::getline(iss, ifname);
            ifname = trim_left(ifname);
            if (name.empty() || ifname.empty()) {
                std::cout << "usage: create <name> <ifname>\n";
                continue;
            }
            instances[name] = std::make_unique<UniTransmit>(ifname);
            std::cout << "created " << name << " scheme=" << instances[name]->scheme();
            print_instance_state(*instances[name]);
            std::cout << "\n";
            continue;
        }
        if (cmd == "delete") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: delete <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
            } else {
                instances.erase(it);
                std::cout << "deleted\n";
            }
            continue;
        }
        if (cmd == "start") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: start <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            it->second->start();
            std::cout << "started";
            print_instance_state(*it->second);
            std::cout << "\n";
            continue;
        }
        if (cmd == "close") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: close <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            it->second->close();
            std::cout << "closed\n";
            continue;
        }
        if (cmd == "write") {
            std::string name;
            iss >> name;
            std::string payload;
            std::getline(iss, payload);
            payload = trim_left(payload);
            if (name.empty()) {
                std::cout << "usage: write <name> <text>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            std::size_t written = it->second->write(payload);
            std::cout << "written=" << written;
            print_instance_state(*it->second);
            std::cout << "\n";
            continue;
        }
        if (cmd == "read") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: read <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            auto data = it->second->read();
            print_bytes(data);
            continue;
        }
        if (cmd == "readn") {
            std::string name;
            std::size_t n = 0;
            iss >> name >> n;
            if (name.empty() || n == 0) {
                std::cout << "usage: readn <name> <n>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            auto data = it->second->read(n);
            print_bytes(data);
            continue;
        }
        if (cmd == "readall") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: readall <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            auto data = it->second->read_all();
            print_bytes(data);
            continue;
        }
        if (cmd == "in_waiting") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: in_waiting <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            std::cout << it->second->in_waiting() << "\n";
            continue;
        }
        if (cmd == "on_recv") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: on_recv <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            it->second->set_receive_callback(
                [name](const std::vector<std::uint8_t> &data,
                       const UniTransmit::ReceiveContext &ctx) {
                    std::cout << "[recv " << name << " " << ctx.scheme << "] ";
                    print_bytes(data);
                });
            std::cout << "callback enabled\n";
            continue;
        }
        if (cmd == "off_recv") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "usage: off_recv <name>\n";
                continue;
            }
            auto it = instances.find(name);
            if (it == instances.end()) {
                std::cout << "not found\n";
                continue;
            }
            it->second->set_receive_callback(nullptr);
            std::cout << "callback disabled\n";
            continue;
        }

        std::cout << "unknown command\n";
    }

    return 0;
}
