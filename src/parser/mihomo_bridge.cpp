#include "mihomo_bridge.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <functional>
#include <sys/wait.h>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace mihomo {

std::string ProxyNode::toYAML() const {
  std::stringstream ss;
  ss << "  - name: \"" << name << "\"\n";
  ss << "    type: " << type << "\n";
  ss << "    server: " << server << "\n";
  ss << "    port: " << port << "\n";

  for (const auto &[key, value] : params) {
    ss << "    " << key << ": " << value << "\n";
  }

  return ss.str();
}

std::vector<ProxyNode> parseSubscription(const std::string &subscription) {
  std::vector<ProxyNode> nodes;

  // Write subscription to a temp file for the helper to read
  std::string tmpfile = "/tmp/mihomo_sub_" + std::to_string(std::hash<std::string>{}(subscription));
  {
    FILE *tf = fopen(tmpfile.c_str(), "we");
    if (tf) {
      fwrite(subscription.data(), 1, subscription.size(), tf);
      fclose(tf);
    }
  }

  // Call mihomo_helper subprocess via pipe, feeding stdin from temp file and capturing stderr
  std::string cmd = "mihomo_helper < " + tmpfile + " 2>&1";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    // Fallback: try relative path
    cmd = "./mihomo_helper < " + tmpfile + " 2>&1";
    pipe = popen(cmd.c_str(), "r");
  }
  if (!pipe) {
    throw std::runtime_error("Failed to start mihomo_helper process");
  }

  // Read result from stdout (includes stderr due to 2>&1)
  std::string result;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    result += buffer;
  }

  int raw_status = pclose(pipe);
  int exit_code = WIFEXITED(raw_status) ? WEXITSTATUS(raw_status) : -1;

  // Clean up temp file
  remove(tmpfile.c_str());

  // Parse output: mihomo_helper outputs JSON array on success, {"error":"..."} on failure
  if (result.empty()) {
    throw std::runtime_error("Mihomo parser error: no output from helper process (exit code " +
                             std::to_string(exit_code) + ")");
  }

  try {
    auto json_result = nlohmann::json::parse(result);
    if (json_result.contains("error")) {
      throw std::runtime_error("Mihomo parser error: " + json_result["error"].get<std::string>());
    }

    // Parse proxy array
    for (const auto &item : json_result) {
      ProxyNode node;
      node.name = item.value("name", "");
      node.type = item.value("type", "");
      node.server = item.value("server", "");

      if (item.contains("port")) {
        if (item["port"].is_number()) {
          node.port = item["port"].get<int>();
        } else if (item["port"].is_string()) {
          try {
            node.port = std::stoi(item["port"].get<std::string>());
          } catch (...) {
            node.port = 0;
          }
        } else {
          node.port = 0;
        }
      } else {
        node.port = 0;
      }

      for (auto it = item.begin(); it != item.end(); ++it) {
        const std::string &key = it.key();
        if (key != "name" && key != "type" && key != "server" &&
            key != "port") {
          std::string value;
          if (it->is_string()) {
            value = it->get<std::string>();
          } else if (it->is_number_integer()) {
            value = std::to_string(it->get<int>());
          } else if (it->is_number_float()) {
            value = std::to_string(it->get<double>());
          } else if (it->is_boolean()) {
            value = it->get<bool>() ? "true" : "false";
          } else {
            value = it->dump();
          }
          node.params[key] = value;
        }
      }

      nodes.push_back(node);
    }

    return nodes;

  } catch (const nlohmann::json::exception &) {
    if (exit_code != 0) {
      throw std::runtime_error("Mihomo parser error: helper process failed with exit code " +
                               std::to_string(exit_code) + ", output: " + result);
    }
    throw std::runtime_error("Mihomo parser error: invalid JSON output: " + result);
  }

  return nodes;
}

bool isMihomoParserAvailable() {
  // Check if helper binary exists
  FILE *pipe = popen("command -v mihomo_helper 2>/dev/null || echo ''", "r");
  if (pipe) {
    char buf[256] = {};
    fgets(buf, sizeof(buf), pipe);
    pclose(pipe);
    if (buf[0] != '\0')
      return true;
  }
  return false;
}

} // namespace mihomo
