// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <stack>
#include <tuple>
#include <string>
#include <string_view>

#include <QtCore/QXmlStreamAttributes>

#include "stree_manager.h"

namespace nx::utils::stree {

class AbstractNode;

namespace MatchType
{
    enum Value
    {
        unknown,
        equal,
        greater,
        less,
        wildcard,
        presence, //< true if resource is present in input set.
        range,
    };

    NX_UTILS_API Value fromString(const std::string_view& str);
} // namespace MatchType

class NX_UTILS_API SaxHandler
{
public:
    SaxHandler(const ResourceNameSet& resourceNameSet);
    virtual ~SaxHandler();

    bool startDocument();

    bool startElement(
        const QStringRef& namespaceUri,
        const QStringRef& name,
        const QXmlStreamAttributes& atts);

    bool endDocument();

    bool endElement(
        const QStringRef& namespaceUri,
        const QStringRef& localName);

    bool characters(const QStringRef&);

    QString errorString() const;

    void setFlags(int flags);

    /** @returns root of tree, created during parsing xml. */
    const std::unique_ptr<AbstractNode>& root() const;

    /**
     * Releases ownership of the tree, created during parsing.
     */
    std::unique_ptr<AbstractNode> releaseTree();

private:
    enum State
    {
        buildingTree,
        skippingNode
    };

    enum class ResultCode
    {
        ok,
        missingAttribute,
        unknownResource,
        unknownNodeType,
        other,
    };

    std::stack<AbstractNode*> m_nodes;
    mutable QString m_errorDescription;
    State m_state = buildingTree;
    int m_inlineLevel = 0;
    std::unique_ptr<AbstractNode> m_root;
    const ResourceNameSet& m_resourceNameSet;
    int m_flags = 0;

    std::tuple<std::unique_ptr<AbstractNode>, ResultCode> createNode(
        const QString& nodeName,
        const QXmlStreamAttributes& atts) const;

    template<typename T>
    std::unique_ptr<AbstractNode> createConditionNode(
        MatchType::Value matchType,
        int matchResID) const;

    std::unique_ptr<AbstractNode> createConditionNodeForStringRes(
        MatchType::Value matchType,
        int matchResID) const;
};

} // namespace nx::utils::stree
