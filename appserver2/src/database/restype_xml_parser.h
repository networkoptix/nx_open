#ifndef __RES_TYPE_XML_PARSER_H
#define __RES_TYPE_XML_PARSER_H

#include <QtXml/QXmlDefaultHandler>
#include "nx_ec/data/api_fwd.h"

namespace ec2
{

class ResTypeXmlParser: public QXmlDefaultHandler
{
public:
    ResTypeXmlParser(ApiResourceTypeDataList& data);

    virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& attrs ) override;
    virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName ) override;
private:
    const ApiResourceTypeData* findResTypeByName(const QString& name);
    bool addParentType(ApiResourceTypeData& data, const QString& parentName);
    bool processResource(const QString& localName, const QXmlAttributes& attrs);
    bool processParam(const QString& localName, const QXmlAttributes& attrs);
private:
    ApiResourceTypeDataList& m_data;
    QString m_vendor;
    bool m_resTypeFound;
};

}

#endif // __RES_TYPE_XML_PARSER_H
