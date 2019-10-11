#include "cursor.h"

namespace nx::analytics::db {

Cursor::Cursor(
    std::unique_ptr<nx::sql::Cursor<ObjectTrack>> sqlCursor)
    :
    m_sqlCursor(std::move(sqlCursor))
{
}

Cursor::~Cursor()
{
    if (m_onBeforeCursorDestroyedHandler)
        m_onBeforeCursorDestroyedHandler(this);
}

common::metadata::ConstObjectMetadataPacketPtr Cursor::next()
{
    while (!m_eof || !m_currentTracks.empty())
    {
        loadObjectsIfNecessary();

        auto [object, trackPositionIndex] = readNextTrackPosition();
        if (!object)
            break;

        if (!m_packet)
        {
            m_packet = createMetaDataPacket(*object, (int) trackPositionIndex);
            continue;
        }

        if (m_packet && canAddToTheCurrentPacket(*object, (int) trackPositionIndex))
        {
            addToCurrentPacket(*object, (int) trackPositionIndex);
            continue;
        }

        auto curPacket = std::exchange(m_packet, nullptr);
        m_packet = createMetaDataPacket(*object, (int) trackPositionIndex);
        return curPacket;
    }

    return std::exchange(m_packet, nullptr);
}

void Cursor::close()
{
    QnMutexLocker lock(&m_mutex);
    m_sqlCursor.reset();
    m_eof = true;
}

void Cursor::setOnBeforeCursorDestroyed(
    nx::utils::MoveOnlyFunc<void(Cursor*)> handler)
{
    m_onBeforeCursorDestroyedHandler = std::move(handler);
}

std::tuple<const ObjectTrack*, std::size_t /*track position*/> Cursor::readNextTrackPosition()
{
    // Selecting track position with minimal timestamp.
    auto it = std::get<0>(findTrackPosition());
    if (it == m_currentTracks.end())
        return std::make_tuple(nullptr, 0);

    const auto result = std::make_tuple(&it->first, it->second);
    ++it->second;
    return result;
}

void Cursor::loadObjectsIfNecessary()
{
    // Always storing the next track in advance.
    // I.e., the one that has track_start_time larger than the current track position.

    while (!m_eof &&
        (m_currentTracks.empty() ||
            nextTrackPositionTimestamp() >= maxObjectTrackStartTimestamp()))
    {
        loadNextTrack();
    }
}

void Cursor::loadNextTrack()
{
    QnMutexLocker lock(&m_mutex);

    if (m_eof)
        return;

    auto track = m_sqlCursor->next();
    if (!track)
    {
        m_eof = true;
        return;
    }

    m_currentTracks.emplace_back(std::move(*track), 0);
}

qint64 Cursor::nextTrackPositionTimestamp()
{
    return std::get<1>(findTrackPosition());
}

std::tuple<Cursor::Tracks::iterator /*track*/, qint64 /*track position timestamp*/>
    Cursor::findTrackPosition()
{
    // Selecting track position with minimal timestamp.
    qint64 currentMinTimestamp = std::numeric_limits<qint64>::max();
    int pos = -1;
    for (auto it = m_currentTracks.begin(); it != m_currentTracks.end(); )
    {
        if (it->second == it->first.objectPositionSequence.size())
        {
            it = m_currentTracks.erase(it);
            continue;
        }

        if (it->first.objectPositionSequence[it->second].timestampUs < currentMinTimestamp)
        {
            currentMinTimestamp = it->first.objectPositionSequence[it->second].timestampUs;
            pos = it - m_currentTracks.begin();
        }

        ++it;
    }

    return std::make_tuple(
        pos >= 0 ? (m_currentTracks.begin() + pos) : m_currentTracks.end(),
        currentMinTimestamp);
}

qint64 Cursor::maxObjectTrackStartTimestamp()
{
    qint64 result = std::numeric_limits<qint64>::min();
    for (const auto& track: m_currentTracks)
        result = std::max(result, track.first.firstAppearanceTimeUs);
    return result;
}

common::metadata::ObjectMetadataPacketPtr Cursor::createMetaDataPacket(
    const ObjectTrack& track,
    int trackPositionIndex)
{
    auto packet = std::make_shared<common::metadata::ObjectMetadataPacket>();

    packet->deviceId = track.objectPositionSequence[trackPositionIndex].deviceId;
    packet->timestampUs = track.objectPositionSequence[trackPositionIndex].timestampUs;
    packet->durationUs = track.objectPositionSequence[trackPositionIndex].durationUs;
    packet->objectMetadataList.push_back(toMetadataObject(track, trackPositionIndex));
    return packet;
}

nx::common::metadata::ObjectMetadata Cursor::toMetadataObject(
    const ObjectTrack& track,
    int trackPositionIndex)
{
    nx::common::metadata::ObjectMetadata result;
    result.typeId = track.objectTypeId;
    result.trackId = track.id;
    result.boundingBox = track.objectPositionSequence[trackPositionIndex].boundingBox;
    result.attributes = track.attributes;
    return result;
}

bool Cursor::canAddToTheCurrentPacket(
    const ObjectTrack& track,
    int trackPositionIndex) const
{
    return m_packet->deviceId == track.deviceId
        && m_packet->timestampUs == track.objectPositionSequence[trackPositionIndex].timestampUs;
}

void Cursor::addToCurrentPacket(
    const ObjectTrack& track,
    int trackPositionIndex)
{
    m_packet->objectMetadataList.push_back(toMetadataObject(track, trackPositionIndex));
    m_packet->durationUs = std::max(
        m_packet->durationUs,
        track.objectPositionSequence[trackPositionIndex].durationUs);
}

} // namespace nx::analytics::db
