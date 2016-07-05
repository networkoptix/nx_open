/**********************************************************
* 28 sep 2013
* a.kolesnikov
***********************************************************/

#include "mirror_list_xml_parse_handler.h"


static const QString mirrorListTagName( QLatin1String("mirrorlist") );
static const QString productTagName( QLatin1String("product") );
static const QString customizationTagName( QLatin1String("customization") );
static const QString moduleTagName( QLatin1String("module") );
static const QString versionTagName( QLatin1String("version") );
static const QString mirrorTagName( QLatin1String("mirror") );

MirrorListXmlParseHandler::MirrorListXmlParseHandler(
    const QString& productName,
    const QString& customization,
    const QString& version,
    const QString& module,
    std::forward_list<QUrl>* const mirrorList )
:
    m_productName( productName ),
    m_customization( customization ),
    m_version( version ),
    m_module( module ),
    m_mirrorList( mirrorList ),
    m_matchedPath( true )
{
}

bool MirrorListXmlParseHandler::endDocument()
{
    m_mirrorList->reverse();
    return true;
}

//!Implementation of QXmlDefaultHandler::startElement
bool MirrorListXmlParseHandler::startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
{
    const QString nameAttrValue = atts.value( "name" );

    if( m_currentPath.empty() )
    {
        if( qName != mirrorListTagName )
            return false;
    }
    else if( m_currentPath.top() == mirrorListTagName )
    {
        if( qName != productTagName )
            return false;
        if( nameAttrValue != m_productName )
            m_matchedPath = false;
    }
    else if( m_currentPath.top() == productTagName )
    {
        if( qName != customizationTagName )
            return false;
        if( nameAttrValue != m_customization )
            m_matchedPath = false;
    }
    else if( m_currentPath.top() == customizationTagName )
    {
        if( qName != moduleTagName )
            return false;
        if( nameAttrValue != m_module )
            m_matchedPath = false;
    }
    else if( m_currentPath.top() == moduleTagName )
    {
        if( qName != versionTagName )
            return false;
        if( nameAttrValue != m_version )
            m_matchedPath = false;
    }
    else if( m_currentPath.top() == versionTagName )
    {
        if( qName != mirrorTagName )
            return false;
    }
    else
    {
        return false;
    }

    m_currentPath.push( qName );
    return true;
}

//!Implementation of QXmlDefaultHandler::endElement
bool MirrorListXmlParseHandler::endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
{
    if( m_currentPath.empty() )
        return false;

    m_currentPath.pop();
    return true;
}

//!Implementation of QXmlDefaultHandler::characters
bool MirrorListXmlParseHandler::characters( const QString& ch )
{
    if( (m_currentPath.top() == mirrorTagName) && m_matchedPath )
        m_mirrorList->push_front( ch );
    return true;
}

//!Implementation of QXmlDefaultHandler::errorString
QString MirrorListXmlParseHandler::errorString() const
{
    return m_errorText;
}

//!Implementation of QXmlDefaultHandler::error
bool MirrorListXmlParseHandler::error( const QXmlParseException& exception )
{
    m_errorText = QString::fromLatin1("Parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

//!Implementation of QXmlDefaultHandler::fatalError
bool MirrorListXmlParseHandler::fatalError( const QXmlParseException& exception )
{
    m_errorText = QString::fromLatin1("Parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}
