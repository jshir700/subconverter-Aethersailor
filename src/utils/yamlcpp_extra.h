#ifndef YAMLCPP_EXTRA_H_INCLUDED
#define YAMLCPP_EXTRA_H_INCLUDED

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

template <typename T> void operator >> (const YAML::Node& node, T& i)
{
    if(node.IsDefined() && !node.IsNull()) //fail-safe
        i = node.as<T>();
};

template <typename T> T safe_as (const YAML::Node& node)
{
    if(node.IsDefined() && !node.IsNull())
        return node.as<T>();
    return T();
};

template <typename T> void operator >>= (const YAML::Node& node, T& i)
{
    i = safe_as<T>(node);
};

using string_array = std::vector<std::string>;

inline std::string dump_to_pairs (const YAML::Node &node, const string_array &exclude = string_array())
{
    std::string result;
    for(auto iter = node.begin(); iter != node.end(); iter++)
    {
        if(iter->second.Type() != YAML::NodeType::Scalar)
            continue;
        std::string key = iter->first.as<std::string>();
        if(std::find(exclude.cbegin(), exclude.cend(), key) != exclude.cend())
            continue;
        std::string value = iter->second.as<std::string>();
        result += key + "=" + value + ",";
    }
    return result.erase(result.size() - 1);
}

/// Create a YAML scalar node with double-quoted output, preserving the raw value.
/// This is used to ensure UA values like "mihomo/1.18.3" are output as
///   header:
///     User-Agent: "mihomo/1.18.3"
/// instead of the default (unquoted) YAML output which would produce
///   header:
///     User-Agent: mihomo/1.18.3
/// which is invalid YAML.
inline YAML::Node make_yaml_quoted_scalar(const std::string &value)
{
    YAML::Node node(YAML::NodeType::Scalar);
    node.SetTag("?");
#if YAML_CPP_API_VERSION >= 40000
    // yaml-cpp 0.8+ uses EmitterStyle
    node.SetStyle(YAML::EmitterStyle::DoubleQuoted);
#else
    // older yaml-cpp (<0.8) uses ScalarStyle
    node.SetScalar(value);
    node.SetStyle(YAML::EmitterStyle::DoubleQuoted);
#endif
    return node;
}

#endif // YAMLCPP_EXTRA_H_INCLUDED
