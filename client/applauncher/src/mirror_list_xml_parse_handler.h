/**********************************************************
* 28 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef MIRROR_LIST_XML_PARSE_HANDLER_H
#define MIRROR_LIST_XML_PARSE_HANDLER_H

#include <forward_list>
#include <stack>

#include <QtCore/QUrl>
#include <QtXml/QXmlDefaultHandler>


//!Parses xml with product download mirror list and selects mirrors for given key [product;customization;module;version]
/*!
    Xml format is described in readme.txt
*/
class MirrorListXmlParseHandler
:
    public QXmlDefaultHandler
{
public:
    //!Initialization
    /*!
        \param mirrorList Filled with matcher mirror urls
    */
    MirrorListXmlParseHandler(
        const QString& productName,
        const QString& customization,
        const QString& version,
        const QString& module,
        std::forward_list<QUrl>* const mirrorList );

    //!Implementation of QXmlDefaultHandler::endDocument
    virtual bool endDocument() override;
    //!Implementation of QXmlDefaultHandler::startElement
    virtual bool startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts ) override;
    //!Implementation of QXmlDefaultHandler::endElement
    virtual bool endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName ) override;
    //!Implementation of QXmlDefaultHandler::characters
    virtual bool characters( const QString& ch ) override;
    //!Implementation of QXmlDefaultHandler::errorString
    virtual QString errorString() const override;
    //!Implementation of QXmlDefaultHandler::error
    virtual bool error( const QXmlParseException& exception ) override;
    //!Implementation of QXmlDefaultHandler::fatalError
    virtual bool fatalError( const QXmlParseException& exception ) override;

private:
    const QString& m_productName;
    const QString& m_customization;
    const QString& m_version;
    const QString& m_module;
    std::forward_list<QUrl>* const m_mirrorList;
    std::stack<QString> m_currentPath;
    bool m_matchedPath;
    QString m_errorText;
};

#endif  //MIRROR_LIST_XML_PARSE_HANDLER_H
