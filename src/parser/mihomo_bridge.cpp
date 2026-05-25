#include "mihomo_bridge.h"
#include <nlohmann/json.hpp>
#include <cstdio>
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

  // Call mihomo_helper subprocess via pipe
  std::string cmd = "mihomo_helper";
  FILE *pipe = popen(cmd.c_str(), "rwe");
  if (!pipe) {
    // Fallback: try relative path
    pipe = popen("./mihomo_helper", "rwe");
  }
  if (!pipe) {
    throw std::runtime_error("Failed to start mihomo_helper process");
  }

  // Write subscription data to stdin
  fwrite(subscription.data(), 1, subscription.size(), pipe);
  fflush(pipe);

  // Read JSON result from stdout
  std::string result;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    result += buffer;
  }

  int exit_code = pclose(pipe);
  if (exit_code != 0 || result.empty()) {
    throw std::runtime_error("Mihomo parser error: helper process failed with exit code " +
                             std::to_string(exit_code));
  }

  // Parse JSON result
  try {
    auto json_result = nlohmann::json::parse(result);

    // Check for error
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

  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error(std::string("JSON parse error: ") + e.what());
  }

  return nodes;
}

bool isMihomoParserAvailable() {
  // Check if helper binary exists
  FILE *pipe = popen("command -v mihomo_helper 2>/dev/null || echo ''", "re");
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
