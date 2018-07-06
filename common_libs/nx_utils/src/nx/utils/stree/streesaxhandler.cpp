#include "streesaxhandler.h"

#include <map>
#include <memory>

#include <nx/utils/std/cpp14.h>

#include "node.h"
#include "range_match_container.h"
#include "streecontainer.h"
#include "wildcardmatchcontainer.h"

namespace nx {
namespace utils {
namespace stree {

namespace MatchType {
    Value fromString(const QString& str)
    {
        if (str == lit("equal"))
            return equal;
        else if (str == lit("greater"))
            return greater;
        else if (str == lit("less"))
            return less;
        else if (str == lit("wildcard"))
            return wildcard;
        else if (str == lit("presence"))
            return presence;
        else if (str == lit("range"))
            return range;
        else
            return unknown;
    }
} // namespace MatchType

SaxHandler::SaxHandler(const ResourceNameSet& resourceNameSet):
    m_state(buildingTree),
    m_inlineLevel(0),
    m_resourceNameSet(resourceNameSet)
{
}

SaxHandler::~SaxHandler()
{
}

bool SaxHandler::startDocument()
{
    m_root.reset();

    return true;
}

bool SaxHandler::startElement(
    const QString& /*namespaceURI*/,
    const QString& /*localName*/,
    const QString& qName,
    const QXmlAttributes& atts)
{
    if (m_state == skippingNode)
    {
        ++m_inlineLevel;
        return true;
    }

    std::unique_ptr<AbstractNode> newNode(createNode(qName, atts));
    if (!newNode.get())
        return false;
    auto newNodePtr = newNode.get();

    int valuePos = atts.index(lit("value"));
    if (m_nodes.empty())
    {
        m_root = std::move(newNode);
    }
    else if (!m_nodes.top()->addChild(
        valuePos == -1 ? QVariant() : QVariant(atts.value(valuePos)),
        std::move(newNode)))
    {
        m_state = skippingNode;
        m_inlineLevel = 1;
        return true;
    }

    m_nodes.emplace(newNodePtr);
    return true;
}

bool SaxHandler::endDocument()
{
    return true;
}

bool SaxHandler::endElement(
    const QString& /*namespaceURI*/,
    const QString& /*localName*/,
    const QString& /*qName*/)
{
    if (m_state == skippingNode)
    {
        --m_inlineLevel;
        if (m_inlineLevel == 0)
            m_state = buildingTree;
        return true;
    }

    m_nodes.pop();

    return true;
}

QString SaxHandler::errorString() const
{
    return m_errorDescription;
}

bool SaxHandler::error(const QXmlParseException& exception)
{
    m_errorDescription = lit("Parse error. line %1, col %2, parser message: %3")
        .arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

bool SaxHandler::fatalError(const QXmlParseException& exception)
{
    m_errorDescription = lit("Fatal parse error. line %1, col %2, parser message: %3")
        .arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

const std::unique_ptr<AbstractNode>& SaxHandler::root() const
{
    return m_root;
}

std::unique_ptr<AbstractNode> SaxHandler::releaseTree()
{
    return std::move(m_root);
}

std::unique_ptr<AbstractNode> SaxHandler::createNode(
    const QString& nodeName,
    const QXmlAttributes& atts) const
{
    if (nodeName == lit("condition"))
    {
        //resName, matchType
        int resNamePos = atts.index(lit("resName"));
        if (resNamePos == -1)
        {
            m_errorDescription = 
                lit("No required attribute \"resName\" in ConditionNode");
            return NULL;
        }
        const QString& resName = atts.value(resNamePos);
        const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName(resName);
        if (res.id == -1)
        {
            m_errorDescription = 
                lit("Unknown resource %1 found as \"resName\" attribute of ConditionNode").arg(resName);
            return NULL;
        }

        MatchType::Value matchType = MatchType::equal;
        int matchTypePos = atts.index(lit("matchType"));
        if (matchTypePos >= 0)
        {
            const QString& matchTypeStr = atts.value(matchTypePos);
            matchType = MatchType::fromString(matchTypeStr);
            if (matchType == MatchType::unknown)
            {
                m_errorDescription = lit("ConditionNode does not support match type %1").arg(matchTypeStr);
                return NULL;
            }
        }

        switch (res.type)
        {
            case QVariant::Int:
                return createConditionNode<int>(matchType, res.id);
            case QVariant::UInt:
                return createConditionNode<unsigned int>(matchType, res.id);
            case QVariant::ULongLong:
                return createConditionNode<qulonglong>(matchType, res.id);
            case QVariant::Double:
                return createConditionNode<double>(matchType, res.id);
            case QVariant::String:
                return createConditionNodeForStringRes(matchType, res.id);
            case QVariant::Bool:
                return createConditionNode<bool>(matchType, res.id);
            default:
                m_errorDescription =
                    lm("ConditionNode currently does not support resource of type %1 "
                        "(resource name %2). Only %3 types are supported")
                        .arg(res.type).arg(resName).arg(lit("int, double, string"));
                return NULL;
        }
    }
    else if (nodeName == lit("sequence"))
    {
        return std::make_unique<SequenceNode>();
    }
    else if (nodeName == lit("set"))
    {
        int resNamePos = atts.index(lit("resName"));
        if (resNamePos == -1)
        {
            m_errorDescription = lit("No required attribute \"resName\" in SetNode");
            return NULL;
        }
        const QString& resName = atts.value(resNamePos);
        int resValuePos = atts.index(lit("resValue"));
        if (resValuePos == -1)
        {
            m_errorDescription =
                lit("No required attribute \"resValue\" in SetNode");
            return NULL;
        }

        const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName(resName);
        if (res.id == -1)
        {
            m_errorDescription =
                lit("Unknown resource %1 found as \"resName\" attribute of SetNode").arg(resName);
            return NULL;
        }

        //converting value to appropriate type
        const QString& resValueStr = atts.value(resValuePos);
        QVariant resValue(resValueStr);
        if (!resValue.convert(res.type))
        {
            m_errorDescription =
                lit("Could not convert value %1 of resource %2 to type %3")
                    .arg(resValueStr).arg(resName).arg(res.type);
            return NULL;
        }

        return std::make_unique<SetNode>(res.id, resValue);
    }

    return NULL;
}

template<typename ResValueType>
std::unique_ptr<AbstractNode> SaxHandler::createConditionNode(
    MatchType::Value matchType,
    int matchResID) const
{
    switch (matchType)
    {
        case MatchType::equal:
            return std::make_unique<ConditionNode<ResValueType, EqualMatchContainer>>(matchResID);
        case MatchType::greater:
            return std::make_unique<ConditionNode<ResValueType, MaxLesserMatchContainer>>(matchResID);
        case MatchType::less:
            return std::make_unique<ConditionNode<ResValueType, MinGreaterMatchContainer>>(matchResID);
        case MatchType::presence:
            return std::make_unique<ResPresenceNode>(matchResID);
        case MatchType::range:
            return std::make_unique<ConditionNode<ResValueType, RangeMatchContainer, RangeConverter<ResValueType>>>(matchResID);
        default:
            NX_ASSERT(false);
            return NULL;
    }
}

std::unique_ptr<AbstractNode> SaxHandler::createConditionNodeForStringRes(
    MatchType::Value matchType,
    int matchResID) const
{
    switch (matchType)
    {
        case MatchType::equal:
            return std::make_unique<ConditionNode<QString, EqualMatchContainer>>(matchResID);
        case MatchType::greater:
            return std::make_unique<ConditionNode<QString, MaxLesserMatchContainer>>(matchResID);
        case MatchType::less:
            return std::make_unique<ConditionNode<QString, MinGreaterMatchContainer>>(matchResID);
        case MatchType::wildcard:
            return std::make_unique<ConditionNode<QString, WildcardMatchContainer>>(matchResID);
        case MatchType::presence:
            return std::make_unique<ResPresenceNode>(matchResID);
        case MatchType::range:
            return std::make_unique<ConditionNode<QString, RangeMatchContainer, RangeConverter<QString>>>(matchResID);
        default:
            NX_ASSERT(false);
            return NULL;
    }
}

} // namespace stree
} // namespace utils
} // namespace nx
