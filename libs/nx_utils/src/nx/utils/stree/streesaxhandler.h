// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <stack>
#include <tuple>
#include <string>
#include <string_view>

#include <QtCore/QXmlStreamAttributes>

#include <nx/reflect/enum_instrument.h>

#include "stree_manager.h"

namespace nx::utils::stree {

class AbstractNode;

/**
 * Supported match types. These values can be specified in `matchType` attribute of the `condition`
 * node.
 */
enum class MatchType
{
    unknown,

    /** The first child node with value equal to that of the tested attribute is selected. */
    equal,

    /**
     * Lexicographical comparison. Child node with minimal value greater than that of the
     * tested attribute is selected.
     */
    greater,

    /**
     * Numberic comparison. Child node with minimal value greater than that of the tested
     * attribute is selected.
     */
    intGreater,

    /**
     * Lexicographical comparison. Child node with max value less than that of the
     * tested attribute is selected.
     */
    less,

    /**
     * Numberic comparison. Child node with max value less than that of the
     * tested attribute is selected.
     */
    intLess,

    /**
     * Wildcard test. Value of each child node is treated as a wildcard mask.
     * The attribute value is tested against child node values.
     * The first child node with value (treated as wildcard mask), that satisfies the value of the
     * attribute, is chosen.
     */
    wildcard,

    /**
     * If specified attribite is present in the input set, then child node with value "true"
     * is selected. Otherwise, node with value "false" is chosen.
     */
    presence,

    /**
     * Child node values are treated as string range. E.g., aaa-bbb.
     * So, child node with value-range that includes the tested attribute value is selected.
     */
    range,

    /**
     * Child node values are treated as integer ranges.
     */
    intRange,
};

NX_REFLECTION_INSTRUMENT_ENUM(MatchType,
    unknown,
    equal,
    greater,
    intGreater,
    less,
    intLess,
    wildcard,
    presence,
    range,
    intRange
);

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API SaxHandler
{
public:
    SaxHandler();
    virtual ~SaxHandler();

    bool startDocument();

    bool startElement(
        const QStringView& namespaceUri,
        const QStringView& name,
        const QXmlStreamAttributes& atts);

    bool endDocument();

    bool endElement(
        const QStringView& namespaceUri,
        const QStringView& localName);

    bool characters(const QStringView&);

    QString errorString() const;

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
        unknownNodeType,
        other,
    };

    std::stack<AbstractNode*> m_nodes;
    mutable QString m_errorDescription;
    State m_state = buildingTree;
    int m_inlineLevel = 0;
    std::unique_ptr<AbstractNode> m_root;

    std::tuple<std::unique_ptr<AbstractNode>, ResultCode> createNode(
        const QString& nodeName,
        const QXmlStreamAttributes& atts) const;

    std::unique_ptr<AbstractNode> createConditionNodeForStringRes(
        MatchType matchType,
        const std::string& attrName) const;
};

} // namespace nx::utils::stree
