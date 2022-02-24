// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "streesaxhandler.h"

#include <map>
#include <memory>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include "node.h"
#include "range_match_container.h"
#include "streecontainer.h"
#include "wildcardmatchcontainer.h"

namespace nx::utils::stree {

namespace MatchType {

Value fromString(const std::string_view& str)
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
    m_resourceNameSet(resourceNameSet)
{
}

SaxHandler::~SaxHandler() = default;

bool SaxHandler::startDocument()
{
    m_root.reset();

    return true;
}

bool SaxHandler::startElement(
    const QStringView& /*namespaceUri*/,
    const QStringView& name,
    const QXmlStreamAttributes& attrs)
{
    if (m_state == skippingNode)
    {
        ++m_inlineLevel;
        return true;
    }

    auto [newNode, resultCode] = createNode(name.toString(), attrs);
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

    const auto value = attrs.value("value");
    if (m_nodes.empty())
    {
        m_root = std::move(newNode);
    }
    else if (!m_nodes.top()->addChild(
        value.isEmpty() ? QVariant() : QVariant(value.toString()),
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
    const QStringView& /*namespaceUri*/,
    const QStringView& /*localName*/)
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

bool SaxHandler::characters(const QStringView&)
{
    return true;
}

QString SaxHandler::errorString() const
{
    return m_errorDescription;
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
    const QXmlStreamAttributes& atts) const
{
    if (nodeName == "condition")
    {
        //resName, matchType
        if (!atts.hasAttribute("resName"))
        {
            m_errorDescription = "No required attribute \"resName\" in ConditionNode";
            return {nullptr, ResultCode::missingAttribute};
        }
        const auto resName = atts.value("resName");
        const ResourceNameSet::ResourceDescription& res =
            m_resourceNameSet.findResourceByName(resName.toString().toStdString());
        if (res.id == -1)
        {
            m_errorDescription = QString(
                "Unknown resource %1 found as \"resName\" attribute of ConditionNode").arg(resName);
            return {nullptr, ResultCode::unknownResource};
        }

        MatchType::Value matchType = MatchType::equal;
        if (atts.hasAttribute("matchType"))
        {
            const auto matchTypeStr = atts.value("matchType");
            matchType = MatchType::fromString(matchTypeStr.toString().toStdString());
            if (matchType == MatchType::unknown)
            {
                m_errorDescription = QString("ConditionNode does not support match type %1")
                    .arg(matchTypeStr);
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
                    nx::format("ConditionNode currently does not support resource of type %1 "
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
        if (!atts.hasAttribute("resName"))
        {
            m_errorDescription = QString("No required attribute \"resName\" in SetNode");
            return {nullptr, ResultCode::missingAttribute};
        }
        const auto resName = atts.value("resName");

        if (!atts.hasAttribute("resValue"))
        {
            m_errorDescription = "No required attribute \"resValue\" in SetNode";
            return {nullptr, ResultCode::missingAttribute};
        }

        const ResourceNameSet::ResourceDescription& res =
            m_resourceNameSet.findResourceByName(resName.toString().toStdString());
        if (res.id == -1)
        {
            m_errorDescription =
                QString("Unknown resource %1 found as \"resName\" attribute of SetNode")
                    .arg(resName);
            return {nullptr, ResultCode::unknownResource};
        }

        //converting value to appropriate type
        const auto resValueStr = atts.value("resValue");
        QVariant resValue(resValueStr.toString());
        if (!resValue.convert(res.type))
        {
            m_errorDescription =
                QString("Could not convert value %1 of resource %2 to type %3")
                    .arg(resValueStr).arg(resName).arg(res.type);
            return {nullptr, ResultCode::other};
        }

        return {std::make_unique<SetNode>(res.id, resValue), ResultCode::ok};
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

} // namespace nx::utils::stree
