#pragma once

#include <QtXml/QXmlDefaultHandler>

#include <nx/vms/api/data_fwd.h>

namespace ec2
{

class ResTypeXmlParser: public QXmlDefaultHandler
{
public:
    ResTypeXmlParser(nx::vms::api::ResourceTypeDataList& data);

    virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& attrs ) override;
    virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName ) override;
private:
    const nx::vms::api::ResourceTypeData* findResTypeByName(const QString& name);
    nx::vms::api::ResourceTypeData* getRootResourceType(const QString& type) const;
    bool addParentType(nx::vms::api::ResourceTypeData& data, const QString& parentName);
    bool processResource(const QString& localName, const QXmlAttributes& attrs);
    bool processParam(const QString& localName, const QXmlAttributes& attrs);
private:
    nx::vms::api::ResourceTypeDataList& m_data;
    nx::vms::api::ResourceTypeData* m_rootResType;
    QString m_vendor;
    bool m_resTypeFound;
};

} // namespace ec2
