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

SaxHandler::SaxHandler() = default;
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
    if (!newNode.get())
        return false;
    auto newNodePtr = newNode.get();

    const auto value = attrs.value("value");
    if (m_nodes.empty())
    {
        m_root = std::move(newNode);
    }
    else if (!m_nodes.top()->addChild(value.toString().toStdString(), std::move(newNode)))
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
        const auto resName = atts.value("resName").toString().toStdString();

        MatchType matchType = MatchType::equal;
        if (atts.hasAttribute("matchType"))
        {
            const auto matchTypeStr = atts.value("matchType");
            bool ok = false;
            matchType = nx::reflect::fromString<MatchType>(matchTypeStr.toString().toStdString(), &ok);
            if (!ok)
            {
                m_errorDescription = QString("ConditionNode does not support match type %1")
                    .arg(matchTypeStr);
                return {nullptr, ResultCode::other};
            }
        }

        return {createConditionNodeForStringRes(matchType, resName), ResultCode::ok};
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
        const auto resName = atts.value("resName").toString().toStdString();

        if (!atts.hasAttribute("resValue"))
        {
            m_errorDescription = "No required attribute \"resValue\" in SetNode";
            return {nullptr, ResultCode::missingAttribute};
        }
        const auto resValue = atts.value("resValue").toString().toStdString();

        return {std::make_unique<SetNode>(resName, resValue), ResultCode::ok};
    }

    return {nullptr, ResultCode::unknownNodeType};
}

std::unique_ptr<AbstractNode> SaxHandler::createConditionNodeForStringRes(
    MatchType matchType,
    const std::string& attrName) const
{
    switch (matchType)
    {
        case MatchType::equal:
            return std::make_unique<ConditionNode<std::string, EqualMatchContainer>>(attrName);

        case MatchType::greater:
            return std::make_unique<ConditionNode<std::string, MaxLesserMatchContainer>>(attrName);

        case MatchType::intGreater:
            return std::make_unique<ConditionNode<int, MaxLesserMatchContainer>>(attrName);

        case MatchType::less:
            return std::make_unique<ConditionNode<std::string, MinGreaterMatchContainer>>(attrName);

        case MatchType::intLess:
            return std::make_unique<ConditionNode<int, MinGreaterMatchContainer>>(attrName);

        case MatchType::wildcard:
            return std::make_unique<ConditionNode<std::string, WildcardMatchContainer>>(attrName);

        case MatchType::presence:
            return std::make_unique<AttrPresenceNode>(attrName);

        case MatchType::range:
            return std::make_unique<ConditionNode<std::string, RangeMatchContainer,
                RangeConverter<std::string>>>(attrName);

        case MatchType::intRange:
            return std::make_unique<ConditionNode<int, RangeMatchContainer,
                RangeConverter<int>>>(attrName);

        default:
            NX_ASSERT(false);
            return nullptr;
    }
}

} // namespace nx::utils::stree
