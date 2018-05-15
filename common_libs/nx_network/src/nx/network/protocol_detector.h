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

class AbstractProtocolRule
{
public:
    virtual ~AbstractProtocolRule() = default;

    virtual int minRequiredDataLength() = 0;
    virtual int maxRequiredDataLength() = 0;
    virtual bool match(const nx::Buffer& buf) = 0;
};

class FixedProtocolPrefixRule:
    public AbstractProtocolRule
{
public:
    FixedProtocolPrefixRule(const std::string& prefix):
        m_prefix(prefix)
    {
    }

    virtual int minRequiredDataLength() override
    {
        return (int) m_prefix.size();
    }

    virtual int maxRequiredDataLength() override
    {
        return (int)m_prefix.size();
    }

    virtual bool match(const nx::Buffer& buf) override
    {
        if ((std::size_t) buf.size() < m_prefix.size())
            return false;

        return strncmp(m_prefix.c_str(), buf.constData(), m_prefix.size()) == 0;
    }

private:
    const std::string m_prefix;
};

//-------------------------------------------------------------------------------------------------

enum class DetectionResult
{
    detected,
    needMoreData,
    unknownProtocol, //< Data provided could not be assigned to one of registered protocols.
};

NX_NETWORK_API std::string toString(DetectionResult detectionResult);

template<typename ProtocolDescriptor>
class ProtocolDetector
{
public:
    void registerProtocol(
        std::unique_ptr<AbstractProtocolRule> rule,
        ProtocolDescriptor descriptor)
    {
        m_minRequiredDataLength =
            std::min(m_minRequiredDataLength, rule->minRequiredDataLength());
        m_maxRequiredDataLength =
            std::max(m_maxRequiredDataLength, rule->maxRequiredDataLength());

        m_rules.push_back(std::make_pair(std::move(rule), std::move(descriptor)));
    }

    int minRequiredDataLength() const
    {
        return m_minRequiredDataLength;
    }

    /**
     * @return tuple<result code, detected protocol name>.
     *   Protocol name is valid only with DetectionResult::detected.
     */
    std::tuple<DetectionResult, ProtocolDescriptor*> match(const nx::Buffer& buf)
    {
        if (m_rules.empty())
            return std::make_tuple(DetectionResult::unknownProtocol, nullptr);

        if (buf.size() < m_minRequiredDataLength)
            return std::make_tuple(DetectionResult::needMoreData, nullptr);

        for (auto& rule: m_rules)
        {
            if (rule.first->match(buf))
                return std::make_tuple(DetectionResult::detected, &rule.second);
        }

        return buf.size() >= m_maxRequiredDataLength
            ? std::make_tuple(DetectionResult::unknownProtocol, nullptr)
            : std::make_tuple(DetectionResult::needMoreData, nullptr);
    }

private:
    int m_minRequiredDataLength = std::numeric_limits<int>::max();
    int m_maxRequiredDataLength = std::numeric_limits<int>::min();
    std::vector<std::pair<std::unique_ptr<AbstractProtocolRule>, ProtocolDescriptor>> m_rules;
};

} // namespace network
} // namespace nx
