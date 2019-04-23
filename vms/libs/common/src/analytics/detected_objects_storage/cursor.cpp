#include "cursor.h"

namespace nx::analytics::storage {

Cursor::Cursor(
    std::unique_ptr<nx::sql::Cursor<DetectedObject>> sqlCursor)
    :
    m_sqlCursor(std::move(sqlCursor))
{
}

common::metadata::ConstDetectionMetadataPacketPtr Cursor::next()
{
    // Always storing the next detected object in advance.
    // I.e., the one that has track_start_time larger than the current track position.

    while (!m_eof || !m_currentObjects.empty())
    {
        auto [object, trackPositionIndex] = readNextTrackPosition();
        if (!object)
        {
            loadNextObject();
            continue;
        }

        if (!m_packet)
        {
            m_packet = createMetaDataPacket(*object, trackPositionIndex);
            continue;
        }

        if (m_packet && canAddToTheCurrentPacket(*object, trackPositionIndex))
        {
            addToCurrentPacket(*object, trackPositionIndex);
            continue;
        }

        auto curPacket = std::exchange(m_packet, nullptr);
        m_packet = createMetaDataPacket(*object, trackPositionIndex);
        return curPacket;
    }

    return std::exchange(m_packet, nullptr);
}

void Cursor::close()
{
    // TODO
}

std::tuple<const DetectedObject*, std::size_t /*track position*/> Cursor::readNextTrackPosition()
{
    std::tuple<const DetectedObject*, std::size_t /*track position*/> result;

    // Selecting track position with minimal timestamp.
    qint64 currentMinTimestamp = std::numeric_limits<qint64>::max();
    int pos = -1;
    for (auto it = m_currentObjects.begin(); it != m_currentObjects.end(); )
    {
        if (it->second == it->first.track.size())
        {
            it = m_currentObjects.erase(it);
            continue;
        }

        if (it->first.track[it->second].timestampUsec < currentMinTimestamp)
        {
            currentMinTimestamp = it->first.track[it->second].timestampUsec;
            result = std::make_tuple(&it->first, it->second);
            pos = it - m_currentObjects.begin();
        }

        ++it;
    }

    if (pos >= 0)
        ++m_currentObjects[pos].second;

    return result;
}

void Cursor::loadNextObject()
{
    auto object = m_sqlCursor->next();
    if (!object)
    {
        m_eof = true;
        return;
    }

    m_currentObjects.emplace_back(std::move(*object), 0);
}

common::metadata::DetectionMetadataPacketPtr Cursor::createMetaDataPacket(
    const DetectedObject& object,
    int trackPositionIndex)
{
    auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();

    packet->deviceId = object.track[trackPositionIndex].deviceId;
    packet->timestampUsec = object.track[trackPositionIndex].timestampUsec;
    packet->durationUsec = object.track[trackPositionIndex].durationUsec;
    packet->objects.push_back(toMetadataObject(object, trackPositionIndex));
    return packet;
}

nx::common::metadata::DetectedObject Cursor::toMetadataObject(
    const DetectedObject& object,
    int trackPositionIndex)
{
    nx::common::metadata::DetectedObject result;
    result.objectTypeId = object.objectTypeId;
    result.objectId = object.objectAppearanceId;
    result.boundingBox = object.track[trackPositionIndex].boundingBox;
    result.labels = object.attributes;
    return result;
}

bool Cursor::canAddToTheCurrentPacket(
    const DetectedObject& object,
    int trackPositionIndex) const
{
    return m_packet->deviceId == object.deviceId
        && m_packet->timestampUsec == object.track[trackPositionIndex].timestampUsec;
}

void Cursor::addToCurrentPacket(
    const DetectedObject& object,
    int trackPositionIndex)
{
    m_packet->objects.push_back(toMetadataObject(object, trackPositionIndex));
    m_packet->durationUsec = std::max(
        m_packet->durationUsec,
        object.track[trackPositionIndex].durationUsec);
}

} // namespace nx::analytics::storage
