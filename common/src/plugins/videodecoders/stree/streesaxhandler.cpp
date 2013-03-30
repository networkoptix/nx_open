////////////////////////////////////////////////////////////
// 28 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streesaxhandler.h"

#include <map>
#include <memory>

#include "streecontainer.h"
#include "wildcardmatchcontainer.h"


namespace stree
{
    SaxHandler::SaxHandler( const ResourceNameSet& resourceNameSet )
    :
        m_state( buildingTree ),
        m_inlineLevel( 0 ),
        m_root( NULL ),
        m_resourceNameSet( resourceNameSet )
    {
    }

    SaxHandler::~SaxHandler()
    {
        //releasing allocated nodes
        if( m_root )
        {
            delete m_root;
            m_root = NULL;
        }
    }

    bool SaxHandler::startDocument()
    {
        if( m_root )
        {
            delete m_root;
            m_root = NULL;
        }

        return true;
    }

    bool SaxHandler::startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
    {
        if( m_state == skippingNode )
        {
            ++m_inlineLevel;
            return true;
        }

        std::auto_ptr<AbstractNode> newNode( createNode( qName, atts ) );
        if( !newNode.get())
            return false;

        int valuePos = atts.index(QString::fromAscii("value"));
        if( !m_nodes.empty() )
            if( !m_nodes.top()->addChild( valuePos == -1 ? QVariant() : QVariant(atts.value(valuePos)), newNode.get() ) )
            {
                m_state = skippingNode;
                m_inlineLevel = 1;
                return true;
            }

        m_nodes.push( newNode.release() );
        return true;
    }

    bool SaxHandler::endDocument()
    {
        return true;
    }

    bool SaxHandler::endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
    {
        if( m_state == skippingNode )
        {
            --m_inlineLevel;
            if( m_inlineLevel == 0 )
                m_state = buildingTree;
            return true;
        }

        if( m_nodes.size() == 1 )
            m_root = m_nodes.top(); //reached tree root
        m_nodes.pop();

        return true;
    }

    QString SaxHandler::errorString() const
    {
        return m_errorDescription;
    }

    bool SaxHandler::error( const QXmlParseException& exception )
    {
        m_errorDescription = QString::fromAscii("Parse error. line %1, col %2, parser message: %3").
            arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
        return false;
    }

    bool SaxHandler::fatalError( const QXmlParseException& exception )
    {
        m_errorDescription = QString::fromAscii("Fatal parse error. line %1, col %2, parser message: %3").
            arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
        return false;
    }

    AbstractNode* SaxHandler::root() const
    {
        return m_root;
    }

    AbstractNode* SaxHandler::releaseTree()
    {
        AbstractNode* rootCopy = m_root;
        m_root = NULL;
        return rootCopy;
    }

    AbstractNode* SaxHandler::createNode( const QString& nodeName, const QXmlAttributes& atts ) const
    {
        if( nodeName == QString::fromAscii("condition") )
        {
            //resName, matchType
            int resNamePos = atts.index(QString::fromAscii("resName"));
            if( resNamePos == -1 )
            {
                m_errorDescription = QString::fromAscii( "No required attribute \"resName\" in ConditionNode" );
                return NULL;
            }
            const QString& resName = atts.value(resNamePos);
            const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName(resName);
            if( res.id == -1 )
            {
                m_errorDescription = QString::fromAscii( "Unknown resource %1 found as \"resName\" attribute of ConditionNode" ).arg(resName);
                return NULL;
            }

            MatchType::Value matchType = MatchType::equal;
            int matchTypePos = atts.index(QString::fromAscii("matchType"));
            if( matchTypePos >= 0 )
            {
                const QString& matchTypeStr = atts.value(matchTypePos);
                matchType = MatchType::fromString( matchTypeStr );
                if( matchType == MatchType::unknown )
                {
                    m_errorDescription = QString::fromAscii( "ConditionNode does not support match type %1" ).arg(matchTypeStr);
                    return NULL;
                }
            }

            switch( res.type )
            {
                case QVariant::Int:
                    return createConditionNode<int>( matchType, res.id );
                case QVariant::UInt:
                    return createConditionNode<unsigned int>( matchType, res.id );
                case QVariant::ULongLong:
                    return createConditionNode<qulonglong>( matchType, res.id );
                case QVariant::Double:
                    return createConditionNode<double>( matchType, res.id );
                case QVariant::String:
                    return createConditionNodeForStringRes( matchType, res.id );
                default:
                    m_errorDescription = QString::fromAscii( "ConditionNode currently does not support resource of type %1 (resource name %2). "
                        "Only %3 types are supported" ).arg(res.type).arg(resName).arg(QString::fromAscii("int, double, string"));
                    return NULL;
            }
        }
        else if( nodeName == QString::fromAscii("sequence") )
        {
            return new SequenceNode();
        }
        else if( nodeName == QString::fromAscii("set") )
        {
            int resNamePos = atts.index(QString::fromAscii("resName"));
            if( resNamePos == -1 )
            {
                m_errorDescription = QString::fromAscii( "No required attribute \"resName\" in SetNode" );
                return NULL;
            }
            const QString& resName = atts.value(resNamePos);
            int resValuePos = atts.index(QString::fromAscii("resValue"));
            if( resValuePos == -1 )
            {
                m_errorDescription = QString::fromAscii( "No required attribute \"resValue\" in SetNode" );
                return NULL;
            }

            const ResourceNameSet::ResourceDescription& res = m_resourceNameSet.findResourceByName( resName );
            if( res.id == -1 )
            {
                m_errorDescription = QString::fromAscii( "Unknown resource %1 found as \"resName\" attribute of SetNode" ).arg(resName);
                return NULL;
            }

            //converting value to appropriate type
            const QString& resValueStr = atts.value(resValuePos);
            QVariant resValue( resValueStr );
            if( !resValue.convert( res.type ) )
            {
                m_errorDescription = QString::fromAscii( "Could not convert value %1 of resource %2 to type %3" ).arg(resValueStr).arg(resName).arg(res.type);
                return NULL;
            }

            return new SetNode( res.id, resValue );
        }

        return NULL;
    }

    template<typename ResValueType> AbstractNode* SaxHandler::createConditionNode( MatchType::Value matchType, int matchResID ) const
    {
        switch( matchType )
        {
            case MatchType::equal:
                return new ConditionNode<std::map<ResValueType, AbstractNode*> >( matchResID );
            case MatchType::greater:
                return new ConditionNode<MaxLesserMatchContainer<ResValueType, AbstractNode*> >( matchResID );
            case MatchType::less:
                return new ConditionNode<MinGreaterMatchContainer<ResValueType, AbstractNode*> >( matchResID );
            default:
                Q_ASSERT(false);
                return NULL;
        }
    }

    AbstractNode* SaxHandler::createConditionNodeForStringRes( MatchType::Value matchType, int matchResID ) const
    {
        switch( matchType )
        {
            case MatchType::equal:
                return new ConditionNode<std::map<QString, AbstractNode*> >( matchResID );
            case MatchType::greater:
                return new ConditionNode<MaxLesserMatchContainer<QString, AbstractNode*> >( matchResID );
            case MatchType::less:
                return new ConditionNode<MinGreaterMatchContainer<QString, AbstractNode*> >( matchResID );
            case MatchType::wildcard:
                return new ConditionNode<WildcardMatchContainer<QString, AbstractNode*> >( matchResID );
            default:
                Q_ASSERT(false);
                return NULL;
        }
    }
}
