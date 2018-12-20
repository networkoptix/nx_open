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
        if (str == "equal")
            return equal;
        if (str == "greater")
            return greater;
        if (str == "less")
            return less;
        if (str == "wildcard")
            return wildcard;
        if (str == "presence")
            return presence;
        if (str == "range")
            return range;
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

    auto[newNode, resultCode] = createNode(qName, atts);
    if ((m_flags & ParseFlag::ignoreUnknownResources) > 0 &&
        resultCode == ResultCode::unknownResource)
    {
        m_state = skippingNode;
        m_inlineLevel = 1;
        return true;
    }

    if (!newNode.get())
        return false;
    auto newNodePtr = newNode.get();

    int valuePos = atts.index("value");
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
    m_errorDescription = QString("Parse error. line %1, col %2, parser message: %3")
        .arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

bool SaxHandler::fatalError(const QXmlParseException& exception)
{
    m_errorDescription = QString("Fatal parse error. line %1, col %2, parser message: %3")
        .arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

void SaxHandler::setFlags(int flags)
{
    m_flags = flags;
}

const std::unique_ptr<AbstractNode>& SaxHandler::root() const
{
    return m_root;
}

std::unique_ptr<AbstractNode> SaxHandler::releaseTree()
{
    return std::move(m_root);
}

std::tuple<std::unique_ptr<AbstractNode>, SaxHandler::ResultCode> SaxHandler::createNode(
    const QString& nodeName,
    const QXmlAttributes& atts ) const
{
    if (nodeName == "condition")
    {
        //resName, matchType
        int resNamePos = atts.index("resName");
        if (resNamePos == -1)
        {
            m_errorDescription = "No required attribute \"resName\" in ConditionNode";
            return {nullptr, ResultCode::missingAttribute};
        }
        const QString& resName = atts.value(resNamePos);
        const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName(resName);
        if (res.id == -1)
        {
            m_errorDescription =
                QString("Unknown resource %1 found as \"resName\" attribute of ConditionNode").arg(resName);
            return {nullptr, ResultCode::unknownResource};
        }

        MatchType::Value matchType = MatchType::equal;
        int matchTypePos = atts.index("matchType");
        if (matchTypePos >= 0)
        {
            const QString& matchTypeStr = atts.value(matchTypePos);
            matchType = MatchType::fromString(matchTypeStr);
            if (matchType == MatchType::unknown)
            {
                m_errorDescription = QString("ConditionNode does not support match type %1").arg(matchTypeStr);
                return {nullptr, ResultCode::other};
            }
        }

        switch (res.type)
        {
            case QVariant::Int:
                return {createConditionNode<int>(matchType, res.id), ResultCode::ok};
            
            case QVariant::UInt:
                return {createConditionNode<unsigned int>(matchType, res.id), ResultCode::ok};
            
            case QVariant::ULongLong:
                return {createConditionNode<qulonglong>(matchType, res.id), ResultCode::ok};

            case QVariant::Double:
                return {createConditionNode<double>(matchType, res.id), ResultCode::ok};

            case QVariant::String:
                return {createConditionNodeForStringRes(matchType, res.id), ResultCode::ok};

            case QVariant::Bool:
                return {createConditionNode<bool>(matchType, res.id), ResultCode::ok};

            default:
                m_errorDescription =
                    lm("ConditionNode currently does not support resource of type %1 "
                        "(resource name %2). Only %3 types are supported")
                        .arg(res.type).arg(resName).arg("int, double, string");
                return {nullptr, ResultCode::other};
        }
    }
    else if (nodeName == "sequence")
    {
        return {std::make_unique<SequenceNode>(), ResultCode::ok};
    }
    else if (nodeName == "set")
    {
        int resNamePos = atts.index("resName");
        if (resNamePos == -1)
        {
            m_errorDescription = QString("No required attribute \"resName\" in SetNode");
            return {nullptr, ResultCode::missingAttribute};
        }
        const QString& resName = atts.value(resNamePos);
        int resValuePos = atts.index("resValue");
        if (resValuePos == -1)
        {
            m_errorDescription = "No required attribute \"resValue\" in SetNode";
            return {nullptr, ResultCode::missingAttribute};
        }

        const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName(resName);
        if (res.id == -1)
        {
            m_errorDescription =
                QString("Unknown resource %1 found as \"resName\" attribute of SetNode")
                    .arg(resName);
            return {nullptr, ResultCode::unknownResource};
        }

        //converting value to appropriate type
        const QString& resValueStr = atts.value(resValuePos);
        QVariant resValue(resValueStr);
        if (!resValue.convert(res.type))
        {
            m_errorDescription =
                QString("Could not convert value %1 of resource %2 to type %3")
                    .arg(resValueStr).arg(resName).arg(res.type);
            return {nullptr, ResultCode::other};
        }

        return {std::make_unique<SetNode>( res.id, resValue ), ResultCode::ok};
    }

    return {nullptr, ResultCode::unknownNodeType};
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
