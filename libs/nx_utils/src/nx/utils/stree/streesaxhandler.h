#pragma once

#include <memory>
#include <stack>
#include <tuple>

#include <QtXml/QXmlDefaultHandler>

#include "resourcenameset.h"

namespace nx {
namespace utils {
namespace stree {

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

    NX_UTILS_API Value fromString(const QString& str);
} // namespace MatchType

enum ParseFlag
{
    ignoreUnknownResources = 0x01,
};

class NX_UTILS_API SaxHandler
:
    public QXmlDefaultHandler
{
public:
    SaxHandler(const ResourceNameSet& resourceNameSet);
    virtual ~SaxHandler();

    virtual bool startDocument();
    virtual bool startElement(
        const QString& namespaceURI,
        const QString& localName,
        const QString& qName,
        const QXmlAttributes& atts);
    virtual bool endDocument();
    virtual bool endElement(
        const QString& namespaceURI,
        const QString& localName,
        const QString& qName);
    virtual QString errorString() const;
    virtual bool error(const QXmlParseException& exception);
    virtual bool fatalError(const QXmlParseException& exception);

    void setFlags(int flags);
    //!Returns root of tree, created during parsing xml
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
    State m_state;
    int m_inlineLevel;
    std::unique_ptr<AbstractNode> m_root;
    const ResourceNameSet& m_resourceNameSet;
    int m_flags = 0;

    std::tuple<std::unique_ptr<AbstractNode>, ResultCode> createNode(
        const QString& nodeName,
        const QXmlAttributes& atts) const;

    template<typename T>
    std::unique_ptr<AbstractNode> createConditionNode(
        MatchType::Value matchType,
        int matchResID) const;

    std::unique_ptr<AbstractNode> createConditionNodeForStringRes(
        MatchType::Value matchType,
        int matchResID) const;
};

} // namespace stree
} // namespace utils
} // namespace nx
