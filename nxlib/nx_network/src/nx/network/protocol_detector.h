#pragma once

#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <nx/utils/std/optional.h>

#include "buffer.h"

namespace nx {
namespace network {

enum class ProtocolMatchResult
{
    detected,
    needMoreData,
    /** Data provided could not be assigned to one of registered protocols. */
    unknownProtocol,
};

NX_NETWORK_API std::string toString(ProtocolMatchResult detectionResult);

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API AbstractProtocolRule
{
public:
    virtual ~AbstractProtocolRule() = default;

    virtual ProtocolMatchResult match(const nx::Buffer& buf) = 0;
};

class NX_NETWORK_API FixedProtocolPrefixRule:
    public AbstractProtocolRule
{
public:
    FixedProtocolPrefixRule(const std::string& prefix);

    virtual ProtocolMatchResult match(const nx::Buffer& buf) override;

private:
    const std::string m_prefix;
};

template<typename ProtocolDescriptor>
class ProtocolDetector
{
public:
    void registerProtocol(
        std::unique_ptr<AbstractProtocolRule> rule,
        ProtocolDescriptor descriptor)
    {
        m_rules.push_back(std::make_pair(std::move(rule), std::move(descriptor)));
    }

    /**
     * @return tuple<result code, detected protocol name>.
     *   Protocol name is valid only with ProtocolMatchResult::detected.
     */
    std::tuple<ProtocolMatchResult, ProtocolDescriptor*> match(const nx::Buffer& buf)
    {
        if (m_rules.empty())
            return std::make_tuple(ProtocolMatchResult::unknownProtocol, nullptr);

        bool needMoreData = false;
        for (auto& rule: m_rules)
        {
            switch (rule.first->match(buf))
            {
                case ProtocolMatchResult::detected:
                    return std::make_tuple(ProtocolMatchResult::detected, &rule.second);

                case ProtocolMatchResult::needMoreData:
                    needMoreData = true;
                    break;

                default:
                    break;
            }
        }

        return needMoreData
            ? std::make_tuple(ProtocolMatchResult::needMoreData, nullptr)
            : std::make_tuple(ProtocolMatchResult::unknownProtocol, nullptr);
    }

private:
    std::vector<std::pair<std::unique_ptr<AbstractProtocolRule>, ProtocolDescriptor>> m_rules;
};

} // namespace network
} // namespace nx
