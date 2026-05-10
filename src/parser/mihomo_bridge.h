#ifndef MIHOMO_BRIDGE_H
#define MIHOMO_BRIDGE_H

#include <map>
#include <string>
#include <vector>
#include "param_compat.h"


namespace mihomo {

/**
 * @brief Proxy node structure parsed from subscription links
 */
struct ProxyNode {
  std::string name;
  std::string type;
  std::string server;
  int port;
  std::map<std::string, std::string> params; // Additional parameters

  // For easier access
  std::string toYAML() const;
};

/**
 * @brief Parse subscription content using mihomo's parser
 *
 * @param subscription Base64-encoded or plain-text subscription data
 * @return Vector of parsed proxy nodes
 * @throws std::runtime_error if parsing fails
 */
std::vector<ProxyNode> parseSubscription(const std::string &subscription);

/**
 * @brief Check if mihomo parser is available
 * @return true if the Go library is properly linked
 */
bool isMihomoParserAvailable();

/**
 * @brief Check if a parameter is supported for a given protocol
 * @param protocol The proxy protocol name (e.g., "ss", "vmess")
 * @param param The parameter name to check
 * @return true if the parameter is supported
 */
bool isParamSupported(const std::string &protocol, const std::string &param);

/**
 * @brief Check if a parameter is hardcoded by mihomo (should not be overridden)
 * @param protocol The proxy protocol name (e.g., "ss", "vmess")
 * @param param The parameter name to check
 * @return true if the parameter is hardcoded
 */
bool isParamHardcoded(const std::string &protocol, const std::string &param);

} // namespace mihomo

#endif // MIHOMO_BRIDGE_H
