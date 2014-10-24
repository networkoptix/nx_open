#include "restype_xml_parser.h"
#include "nx_ec/data/api_resource_type_data.h"

namespace ec2
{

ResTypeXmlParser::ResTypeXmlParser(ApiResourceTypeDataList& data): 
    m_data(data),
    m_resTypeFound(false)
{

}

const ApiResourceTypeData* ResTypeXmlParser::findResTypeByName(const QString& name)
{
    for (int i = m_data.size()-1; i >= 0; --i) {
        if (m_data[i].name == name && m_data[i].vendor == m_vendor)
            return &m_data[i];
    }
    return 0;
}

bool ResTypeXmlParser::addParentType(ApiResourceTypeData& data, const QString& parentName)
{
    const ApiResourceTypeData* parentType = findResTypeByName(parentName);
    if (parentType)
        data.parentId.push_back(parentType->id);
    else
        qWarning() << "Can't find resource type " << parentName << "required for resource type " << data.name;
    return parentType != 0;
}

bool ResTypeXmlParser::processResource(const QString& localName, const QXmlAttributes& attrs)
{
    ApiResourceTypeData newData;
    if (m_vendor.isNull()) {
        qWarning() << "Vendor for resource" << localName << "not found";
        return false;
    }
    newData.name = attrs.value("name").trimmed();
    if (newData.name.isEmpty()) {
        qWarning() << "Invalid XML format. Resource name missing";
        return false;
    }
    newData.vendor = m_vendor;
    if (attrs.value("public").isEmpty()) {
        if (!addParentType(newData, m_vendor + lit(" Camera")))
            return false;
    }
    else {
        for(const QString& parentName: attrs.value("public").split(L',')) 
        {
            if (!addParentType(newData, parentName.trimmed()))
                return false;
        }
    }

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(newData.name.toUtf8());
    md5Hash.addData(m_vendor.toUtf8());
    QByteArray ha2 = md5Hash.result();
    newData.id = QnUuid::fromRfc4122(ha2);

    m_data.push_back(std::move(newData));
    m_resTypeFound = true;

    return true;
}

bool ResTypeXmlParser::processParam(const QString& localName, const QXmlAttributes& attrs)
{
    ApiPropertyTypeData p;
    if (!m_resTypeFound) {
        qWarning() << "Invalid XML format. You should specify resource type before params";
        return false;
    }
    p.name =  attrs.value("name").trimmed();
    if (p.name.isEmpty()) {
        qWarning() << "Invalid XML format. Param name missing";
        return false;
    }
    p.defaultValue =  attrs.value("default_value").trimmed();
    ApiResourceTypeData& data = m_data[m_data.size()-1];
    p.resourceTypeId = data.id;
    data.propertyTypes.push_back(std::move(p));

    return true;
}

bool ResTypeXmlParser::startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& attrs )
{
    Q_UNUSED(namespaceURI)
    Q_UNUSED(qName)

    if (localName == lit("oem"))
        m_vendor = attrs.value("name").trimmed();
    else if (localName == lit("resource"))
        return processResource(localName, attrs);
    else if (localName == lit("param"))
        return processParam(localName, attrs);
    
    return true;
}

bool ResTypeXmlParser::endElement( const QString& namespaceURI, const QString& localName, const QString& qName )
{
    Q_UNUSED(namespaceURI)
    Q_UNUSED(qName)
    
    if (localName == lit("resource"))
        m_resTypeFound = false;

    return true;
}

}
