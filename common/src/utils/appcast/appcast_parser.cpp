#include <QXmlStreamReader>
#include <QDebug>

#include "appcast_parser.h"

namespace {
    static const QString TAG_ITEM(QLatin1String("item"));
    static const QString TAG_ENCLOSURE(QLatin1String("enclosure"));
    static const QString TAG_PLATFORM(QLatin1String("sparkle:os"));
    static const QString TAG_VERSION(QLatin1String("sparkle:version"));
    static const QString TAG_URL(QLatin1String("url"));
    static const QString TAG_TITLE(QLatin1String("title"));
    static const QString TAG_DESCRIPTION(QLatin1String("description"));
    static const QString TAG_PUBDATE(QLatin1String("pubDate"));
};

void QnAppCastParser::setPlatform(const QString& platform) {
    m_platform = platform;
}

QnUpdateInfoItemList QnAppCastParser::items() const {
    return m_items;
}

QnUpdateInfoItemList QnAppCastParser::newItems(const QnVersion& version) const {
    QnUpdateInfoItemList filtered;

    foreach(const QnUpdateInfoItem &item, m_items)
        if (version < item.version)
            filtered.append(item);

    return filtered;
}

void QnAppCastParser::parse(const QByteArray& data) {
    m_items.clear();

    bool inItem = false;
    QnUpdateInfoItem currentItem;

    QXmlStreamReader xml;
    xml.addData(data);

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == TAG_ITEM)
                inItem = true;

            if (inItem && xml.name() == TAG_ENCLOSURE) {
                QXmlStreamAttributes attributes = xml.attributes();

                if (m_platform.isEmpty() || m_platform == attributes.value(TAG_PLATFORM).toString()) {
                    currentItem.url = QUrl(attributes.value(TAG_URL).toString());
                    currentItem.version = attributes.value(TAG_VERSION).toString();
                }
            }

            currentTag = xml.name().toString();
        } else if (xml.isEndElement()) {
            if (xml.name() == TAG_ITEM) {
                if (inItem && !currentItem.isNull())
                    m_items.append(currentItem);
                inItem = false;
                currentItem = QnUpdateInfoItem();
            }
        } else if (inItem && xml.isCharacters() && !xml.isWhitespace()) {
             if (currentTag == TAG_TITLE)
                 currentItem.title = xml.text().toString();
             else if (currentTag == TAG_DESCRIPTION)
                 currentItem.description = xml.text().toString();
             else if (currentTag == TAG_PUBDATE)
                 currentItem.pubDate = xml.text().toString();
        }
     }

     if (xml.error() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
         m_items.clear();

         qWarning() << "XML ERROR:" << xml.lineNumber() << ": " << xml.errorString();
     }
}
