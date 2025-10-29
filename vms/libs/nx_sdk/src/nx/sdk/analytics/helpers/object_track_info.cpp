// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_info.h"

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

IList<ITimestampedObjectMetadata>* ObjectTrackInfo::getTrack() const
{
    if (!m_track)
        return nullptr;

    return shareToPtr(m_track).releasePtr();
}

IUncompressedVideoFrame* ObjectTrackInfo::getBestShotVideoFrame() const
{
    if (!m_bestShotVideoFrame)
        return nullptr;

    return shareToPtr(m_bestShotVideoFrame).releasePtr();
}

ITimestampedObjectMetadata* ObjectTrackInfo::getBestShotObjectMetadata() const
{
    if (!m_bestShotObjectMetadata)
        return nullptr;

    return shareToPtr(m_bestShotObjectMetadata).releasePtr();
}

const char* ObjectTrackInfo::bestShotImageData() const
{
    if (m_bestShotImageData.empty())
        return nullptr;

    return m_bestShotImageData.data();
}

int ObjectTrackInfo::bestShotImageDataSize() const
{
    return (int) m_bestShotImageData.size();
}

const char* ObjectTrackInfo::bestShotImageDataFormat() const
{
    return m_bestShotImageDataFormat.c_str();
}

const char* ObjectTrackInfo::titleText() const
{
    if (m_titleText.empty())
        return nullptr;

    return m_titleText.data();
}

const char* ObjectTrackInfo::titleImageData() const
{
    if (m_titleImageData.empty())
        return nullptr;

    return m_titleImageData.data();
}

int ObjectTrackInfo::titleImageDataSize() const
{
    return (int) m_titleImageData.size();
}

const char* ObjectTrackInfo::titleImageDataFormat() const
{
    return m_titleImageDataFormat.c_str();
}

void ObjectTrackInfo::setTrack(IList<ITimestampedObjectMetadata>* track)
{
    if (!NX_KIT_ASSERT(track))
        return;

    m_track = nx::sdk::shareToPtr(track);
}

void ObjectTrackInfo::setBestShotVideoFrame(IUncompressedVideoFrame* bestShotVideoFrame)
{
    if (!NX_KIT_ASSERT(bestShotVideoFrame))
        return;

    m_bestShotVideoFrame = nx::sdk::shareToPtr(bestShotVideoFrame);
}

void ObjectTrackInfo::setBestShotObjectMetadata(ITimestampedObjectMetadata* bestShotObjectMetadata)
{
    if (!NX_KIT_ASSERT(bestShotObjectMetadata))
        return;

    m_bestShotObjectMetadata = nx::sdk::shareToPtr(bestShotObjectMetadata);
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

void ObjectTrackInfo::setTitleText(std::string titleText)
{
    m_titleText = std::move(titleText);
}
void ObjectTrackInfo::setTitleImageData(std::vector<char> titleImageData)
{
    m_titleImageData = std::move(titleImageData);
}
void ObjectTrackInfo::setTitleImageDataFormat(std::string titleImageDataFormat)
{
    m_titleImageDataFormat = std::move(titleImageDataFormat);
}
void ObjectTrackInfo::setTitleImage(
    std::vector<char> titleImageData,
    std::string titleImageDataFormat)
{
    setTitleImageData(std::move(titleImageData));
    setTitleImageDataFormat(std::move(titleImageDataFormat));
}

} // namespace nx::sdk::analytics
