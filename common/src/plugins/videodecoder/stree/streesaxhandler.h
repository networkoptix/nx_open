////////////////////////////////////////////////////////////
// 28 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREESAXHANDLER_H
#define STREESAXHANDLER_H

#include <memory>
#include <stack>

#include <QtXml/QXmlDefaultHandler>

#include "resourcenameset.h"


namespace stree
{
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
            presence,    //<! \a true if resource is present in input set
            range,
        };

        Value fromString( const QString& str );
    };


    class SaxHandler
    :
        public QXmlDefaultHandler
    {
    public:
        SaxHandler( const ResourceNameSet& resourceNameSet );
        virtual ~SaxHandler();

        virtual bool startDocument();
        virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts );
        virtual bool endDocument();
        virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName );
        virtual QString errorString() const;
        virtual bool error( const QXmlParseException& exception );
        virtual bool fatalError( const QXmlParseException& exception );

        //!Returns root of tree, created during parsing xml
        const std::unique_ptr<AbstractNode>& root() const;
        //!Releases ownership of tree, created during parsing
        std::unique_ptr<AbstractNode> releaseTree();

    private:
        enum State
        {
            buildingTree,
            skippingNode
        };

        std::stack<AbstractNode*> m_nodes;
        mutable QString m_errorDescription;
        State m_state;
        int m_inlineLevel;
        std::unique_ptr<AbstractNode> m_root;
        const ResourceNameSet& m_resourceNameSet;

        std::unique_ptr<AbstractNode> createNode( const QString& nodeName, const QXmlAttributes& atts ) const;
        template<typename T> std::unique_ptr<AbstractNode> createConditionNode( MatchType::Value matchType, int matchResID ) const;
        std::unique_ptr<AbstractNode> createConditionNodeForStringRes( MatchType::Value matchType, int matchResID ) const;
    };
}

#endif  //STREESAXHANDLER_H
