#include "metadata_parser.h"

#include <utility>

#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/utils/log/log_main.h>

#include <QtXml/QDomDocument>

namespace nx::vms_server_plugins::analytics::hikvision {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr double kCoordinateDomain = 1000;
constexpr auto kCacheEntryLifetime = 1s;

} // namespace

Ptr<IMetadataPacket> MetadataParser::parsePacket(QByteArray bytes)
{
    evictStaleCacheEntries();

    // XXX: Strip garbage off both ends. Its presence is possibly a server bug.
    int begin = std::max(0, bytes.indexOf('<'));
    int end = bytes.lastIndexOf('>') + 1;
    bytes = bytes.mid(begin, std::max(0, end - begin));

    QDomDocument xml;
    {
        QString errorDescription;
        int errorLine, errorColumn;
        if (!xml.setContent(bytes, false, &errorDescription, &errorLine, &errorColumn))
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to parse XML: %1 at %2:%3: [[[[%4]]]]",
                errorDescription, errorLine, errorColumn, bytes);
            return nullptr;
        }
    }

    //std::cerr << "-----\n" << xml.toString(2).toStdString() << "-----\n" << std::endl;

    const auto root = xml.documentElement();
    if (root.tagName() != "Metadata")
    {
        NX_WARNING(NX_SCOPE_TAG, "Root is not 'Metadata'");
        return nullptr;
    }

    const auto type = root.firstChildElement("type");
    if (type.isNull())
    {
        NX_WARNING(NX_SCOPE_TAG, "No 'type' in 'Metadata'");
        return nullptr;
    }
    if (type.text() != "activityTarget")
        return nullptr;

    auto packet = makePtr<ObjectMetadataPacket>();

    const auto time = root.firstChildElement("time");
    if (time.isNull())
    {
        NX_WARNING(NX_SCOPE_TAG, "No 'time' in 'Metadata'");
        return nullptr;
    }
    const auto dateTime = QDateTime::fromString(time.text(), Qt::ISODateWithMs);
    if (!dateTime.isValid())
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse ISO timestamp: %1", time.text());
        return nullptr;
    }
    packet->setTimestampUs(dateTime.toMSecsSinceEpoch() * 1000);

    const auto targetDetection = root.firstChildElement("TargetDetection");
    if (targetDetection.isNull())
        return nullptr;

    const auto targetList = targetDetection.firstChildElement("TargetList");
    if (targetList.isNull())
    {
        NX_WARNING(NX_SCOPE_TAG, "No 'TargetList' in 'TargetDetection'");
        return nullptr;
    }

    auto target = targetList.firstChildElement("Target");
    if (target.isNull())
    {
        NX_WARNING(NX_SCOPE_TAG, "No 'Target' in 'TargetList'");
        return nullptr;
    }
    do
    {
        if (auto metadata = parse(target))
            packet->addItem(metadata.get());

        target = target.nextSiblingElement("Target");
    }
    while (!target.isNull());
    if (packet->count() == 0)
        return nullptr;

    return packet;
}

std::optional<Uuid> MetadataParser::parseTrackId(const QString& stringId) const
{
    unsigned int uIntId;
    if (bool ok; uIntId = stringId.toUInt(&ok), !ok)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse unsigned int: %1", stringId);
        return std::nullopt;
    }

    Uuid uuid;
    std::memcpy(uuid.data(), &uIntId, std::min(sizeof(uIntId), (std::size_t) uuid.size()));
    return uuid;
}

QString MetadataParser::parseTypeId(const QString& recognition) const
{
    return NX_FMT("nx.hikvision.%1", recognition);
}

std::optional<Point> MetadataParser::parsePoint(const QDomElement& point) const
{
    double x;
    double y;

    for (const auto [name, ptr]: {
        std::pair{"x", &x},
        std::pair{"y", &y},
    })
    {
        const auto coord = point.firstChildElement(name);
        if (coord.isNull())
        {
            NX_WARNING(NX_SCOPE_TAG, "No '%1' in 'Point'", name);
            return std::nullopt;
        }

        if (bool ok; *ptr = coord.text().toDouble(&ok), !ok)
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to parse double: %1", coord.text());
            return std::nullopt;
        }

        *ptr /= kCoordinateDomain;
    }

    return Point(x, y);
}

std::optional<Rect> MetadataParser::parseBoundingBox(const QDomElement& regionList) const
{
    if (regionList.isNull())
        return std::nullopt;

    Point min = {1, 1};
    Point max = {0, 0};

    for (auto region = regionList.firstChildElement("Region"); !region.isNull();
        region = region.nextSiblingElement("Region"))
    {
        for (auto point = region.firstChildElement("Point"); !point.isNull();
            point = point.nextSiblingElement("Point"))
        {
            if (const auto p = parsePoint(point))
            {
                min.x = std::min(min.x, p->x);
                min.y = std::min(min.y, p->y);

                max.x = std::max(max.x, p->x);
                max.y = std::max(max.y, p->y);
            }
        }
    }

    if (min.x > max.x || min.y > max.y)
    {
        NX_WARNING(NX_SCOPE_TAG, "'RegionList' is empty");
        return std::nullopt;
    }

    Rect rect;
    rect.x = min.x;
    rect.y = min.y;
    rect.width = max.x - min.x;
    rect.height = max.y - min.y;
    return rect;
}

std::optional<std::vector<Ptr<Attribute>>> MetadataParser::parseAttributes(
    const QDomElement& propertyList) const
{
    if (propertyList.isNull())
        return std::nullopt;

    std::vector<Ptr<Attribute>> attributes;
    for (auto property = propertyList.firstChildElement("Property"); !property.isNull();
        property = property.nextSiblingElement("Property"))
    {
        const auto description = property.firstChildElement("description");
        if (description.isNull())
        {
            NX_WARNING(NX_SCOPE_TAG, "No 'description' in 'Property'");
            continue;
        }

        const auto value = property.firstChildElement("value");
        if (value.isNull())
        {
            NX_WARNING(NX_SCOPE_TAG, "No 'value' in 'Property'");
            continue;
        }

        attributes.push_back(makePtr<Attribute>(
            IAttribute::Type::string,
            description.text().toStdString(),
            value.text().toStdString()));
    }
    return attributes;
}

Ptr<IObjectMetadata> MetadataParser::parse(const QDomElement& target)
{
    const auto targetId = target.firstChildElement("targetID");
    if (targetId.isNull())
        return nullptr;

    auto metadata = makePtr<ObjectMetadata>();

    if (const auto trackId = parseTrackId(targetId.text()))
        metadata->setTrackId(*trackId);
    else
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse 'targetID': %1", targetId.text());
        return nullptr;
    }

    const auto recognition = target.firstChildElement("recognition");
    if (recognition.isNull())
        return nullptr;
    metadata->setTypeId(parseTypeId(recognition.text()).toStdString());

    if (const auto boundingBox = parseBoundingBox(target.firstChildElement("RegionList")))
    {
        metadata->setBoundingBox(*boundingBox);

        auto& entry = m_cache[metadata->trackId()];
        entry.boundingBox = *boundingBox;
        entry.lastUpdate = std::chrono::steady_clock::now();
    }
    else if (const auto it = m_cache.find(metadata->trackId()); it != m_cache.end())
    {
        auto& entry = it->second;;

        metadata->setBoundingBox(entry.boundingBox);
    }
    else
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse 'RegionList'");
        return nullptr;
    }

    if (auto attributes = parseAttributes(target.firstChildElement("PropertyList")))
    {
        metadata->addAttributes(*attributes);

        auto& entry = m_cache[metadata->trackId()];
        entry.attributes = std::move(*attributes);
        entry.lastUpdate = std::chrono::steady_clock::now();
    }
    else if (const auto it = m_cache.find(metadata->trackId()); it != m_cache.end())
    {
        auto& entry = it->second;;

        metadata->addAttributes(entry.attributes);
    }
    else
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse 'PropertyList'");
        return nullptr;
    }

    return metadata;
}

void MetadataParser::evictStaleCacheEntries()
{
    const auto now = std::chrono::steady_clock::now();
    for (auto it = m_cache.begin(); it != m_cache.end(); )
    {
        auto& entry = it->second;
        if (now - entry.lastUpdate > kCacheEntryLifetime)
            it = m_cache.erase(it);
        else
            ++it;
    }
}

} // namespace nx::vms_server_plugins::analytics::hikvision
