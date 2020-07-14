#include "metadata_xml_parser.h"

namespace nx::vms_server_plugins::analytics::bosch {

namespace{

[[nodiscard]] QString deleteNamespaces(const QString& xmlText)
{
    QStringList xmlInputItems = xmlText.split('/');
    QStringList xmlOutputItems;
    xmlOutputItems.reserve(xmlInputItems.size());
    for (const QString& item: xmlInputItems)
    {
        QStringList NamespaceAndName = item.split(':');
        xmlOutputItems.push_back(NamespaceAndName[(NamespaceAndName.size() == 1) ? 0 : 1]);
    }
    return xmlOutputItems.join('/');
}

}  // namespace

bool atBranch(const QXmlStreamReader& reader, const char* name)
{
    return reader.name().toString().toLower().trimmed() == name;
}

bool BaseMetadataXmlParser::start(const char* rootTagName)
{
    return next() && isOnTag(rootTagName);
}

bool BaseMetadataXmlParser::next()
{
    return reader.readNextStartElement();
}

void BaseMetadataXmlParser::skip()
{
    reader.skipCurrentElement();
}

bool BaseMetadataXmlParser::isOnTag(const char* tag) const
{
    const QString s = reader.name().toString(); //< for debug
    return reader.name().compare(QByteArray(tag), Qt::CaseInsensitive) == 0;
}

QString BaseMetadataXmlParser::text()
{
    return reader.readElementText().trimmed();
}

QXmlStreamAttributes BaseMetadataXmlParser::attributeList() const
{
    return reader.attributes();
}

ParsedMetadata MetadataXmlParser::parse()
{
    ParsedMetadata result;
    if (!start("MetadataStream"))
        return result;
    {
        while (next())
        {
            switch (knownTagIndex("VideoAnalytics", "Event"))
            {
                case 1:
                    result.objects = parseVideoAnalyticsElement();
                    break;
                case 2:
                    result.events = parseEventElement();
                    break;
            }
        }
    }

    return result;
}

QList<ParsedObject> MetadataXmlParser::parseVideoAnalyticsElement()
{
    QList<ParsedObject> result;
    while (next())
    {
        if (knownTagIndex("Frame"))
        {
            while (next())
            {
                if (knownTagIndex("Object"))
                    result.push_back(parseObjectElement());
            }
        }
    }
    return result;
}

ParsedObject MetadataXmlParser::parseObjectElement()
{
    ParsedObject result;
    result.id = attributeList().value("ObjectId").toInt();
    while (next())
    {
        if (knownTagIndex("appearance"))
        {
            // QString::toFloat uses C locale, that is what we need.
            // If QString is not a float, the result is zero, that is what we need.
            const QXmlStreamAttributes attributes = attributeList();
            result.velocity = attributes.value("velocity").toFloat();
            result.area = attributes.value("area").toFloat();
            while (next())
            {
                if (knownTagIndex("shape"))
                    result.shape = parseShapeElement();
            }
        }
    }

    return result;
}

Shape MetadataXmlParser::parseShapeElement()
{
    Shape result;
    while (next())
    {
//        const QString topic = tag();
        switch (knownTagIndex("BoundingBox", "CenterOfGravity"))
        {
            case 1:
            {
                const QXmlStreamAttributes attributes = attributeList();
                result.boundingBox.bottom = attributes.value("bottom").toFloat();
                result.boundingBox.top = attributes.value("top").toFloat();
                result.boundingBox.right = attributes.value("right").toFloat();
                result.boundingBox.left = attributes.value("left").toFloat();
                skip(); //< Instead of `readElementText`, because we don't need the text.
                break;
            }
            case 2:
            {
                const QXmlStreamAttributes attributes = attributeList();
                result.centerOfGravity.x = attributes.value("x").toFloat();
                result.centerOfGravity.y = attributes.value("y").toFloat();
                skip(); //< Instead of `readElementText`, because we don't need the text.
                break;
            }
        }
    }
    return result;
}

QList<ParsedEvent> MetadataXmlParser::parseEventElement()
{
    QList<ParsedEvent> result;
    while (next())
    {
        if (knownTagIndex("NotificationMessage"))
        {
            result.push_back(parseNotificationMessageElement());
        }
    }
    return result;
}

ParsedEvent MetadataXmlParser::parseNotificationMessageElement()
{
    ParsedEvent result;
    while (next())
    {
//        const QString topic = tag();
        switch (knownTagIndex("Topic", "Message"))
        {
            case 1:
                result.topic = text();
                break;
            case 2:
                // This is the wsnt:Message (oasis message). It contains tt:Message (onvif message).
                while (next())
                {
                    if (knownTagIndex("Message"))
                    {
                        OnvifEventMessage message = parseOnvifMessageElement();
                        if (message.propertyOperation == "Changed")
                            result.isActive = message.state;
                    }
                }
                break;
        }
    }
    return result;
}

OnvifEventMessage MetadataXmlParser::parseOnvifMessageElement()
{
    OnvifEventMessage result;
    result.propertyOperation = attributeList().value("PropertyOperation").toString();
    while (next())
    {
        if (isOnTag("Data"))
        {
            while (next())
            {
                if (isOnTag("SimpleItem"))
                {
                    const QString value = attributeList().value("Value").toString();
                    result.state = (value.toLower() == "true");
                    skip(); //< Instead of `readElementText`, because we don't need the text.
                }
                else
                {
                    skip();
                }
            }
        }
        else
        {
            skip();
        }
    }
    return result;
}

} // namespace nx::vms_server_plugins::analytics::bosch
