// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_info.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

IList<ITimestampedObjectMetadata>* ObjectTrackInfo::getTrack() const
{
    if (!m_track)
        return nullptr;

    m_track->addRef();
    return m_track.get();
}

IUncompressedVideoFrame* ObjectTrackInfo::getBestShotVideoFrame() const
{
    if (!m_bestShotVideoFrame)
        return nullptr;

    m_bestShotVideoFrame->addRef();
    return m_bestShotVideoFrame.get();
}

ITimestampedObjectMetadata* ObjectTrackInfo::getBestShotObjectMetadata() const
{
    if (!m_bestShotObjectMetadata)
        return nullptr;

    m_bestShotObjectMetadata->addRef();
    return m_bestShotObjectMetadata.get();
}

const char* ObjectTrackInfo::bestShotImageData() const
{
    if (m_bestShotImageData.empty())
        return nullptr;

    return m_bestShotImageData.data();
}

int ObjectTrackInfo::bestShotImageDataSize() const
{
    return m_bestShotImageData.size();
}

const char* ObjectTrackInfo::bestShotImageDataFormat() const
{
    return m_bestShotImageDataFormat.c_str();
}

void ObjectTrackInfo::setTrack(IList<ITimestampedObjectMetadata>* track)
{
    if (!NX_KIT_ASSERT(track))
        return;

    track->addRef();
    m_track = nx::sdk::toPtr(track);
}

void ObjectTrackInfo::setBestShotVideoFrame(IUncompressedVideoFrame* bestShotVideoFrame)
{
    if (!NX_KIT_ASSERT(bestShotVideoFrame))
        return;

    bestShotVideoFrame->addRef();
    m_bestShotVideoFrame = nx::sdk::toPtr(bestShotVideoFrame);
}

void ObjectTrackInfo::setBestShotObjectMetadata(ITimestampedObjectMetadata* bestShotObjectMetadata)
{
    if (!NX_KIT_ASSERT(bestShotObjectMetadata))
        return;

    bestShotObjectMetadata->addRef();
    m_bestShotObjectMetadata = nx::sdk::toPtr(bestShotObjectMetadata);
}

void ObjectTrackInfo::setBestShotImageData(std::vector<char> bestShotImageData)
{
    m_bestShotImageData = std::move(bestShotImageData);
}

void ObjectTrackInfo::setBestShotImageDataFormat(std::string bestShotImageDataFormat)
{
    m_bestShotImageDataFormat = std::move(bestShotImageDataFormat);
}

void ObjectTrackInfo::setBestShotImage(
    std::vector<char> bestShotImageData,
    std::string bestShotImageDataFormat)
{
    setBestShotImageData(std::move(bestShotImageData));
    setBestShotImageDataFormat(std::move(bestShotImageDataFormat));
}

} // namespace analytics
} // namespace sdk
} // namespace nx
