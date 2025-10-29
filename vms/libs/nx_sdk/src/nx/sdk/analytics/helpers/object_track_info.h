// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/i_object_track_info.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk::analytics {

class ObjectTrackInfo: public RefCountable<IObjectTrackInfo>
{
public:
    void setTrack(IList<ITimestampedObjectMetadata>* track);
    void setBestShotVideoFrame(IUncompressedVideoFrame* bestShotVideoFrame);
    void setBestShotObjectMetadata(ITimestampedObjectMetadata* bestShotObjectMetadata);
    void setBestShotImageData(std::vector<char> bestShotImageData);
    void setBestShotImageDataFormat(std::string bestShotImageDataFormat);
    void setBestShotImage(
        std::vector<char> bestShotImageData,
        std::string bestShotImageDataFormat);

    virtual const char* bestShotImageData() const override;
    virtual int bestShotImageDataSize() const override;
    virtual const char* bestShotImageDataFormat() const override;

    virtual const char* titleText() const override;
    virtual const char* titleImageData() const override;
    virtual int titleImageDataSize() const override;
    virtual const char* titleImageDataFormat() const override;

    void setTitleText(std::string titleText);
    void setTitleImageData(std::vector<char> titleImageData);
    void setTitleImageDataFormat(std::string titleImageDataFormat);
    void setTitleImage(
        std::vector<char> titleImageData,
        std::string titleImageDataFormat);

protected:
    virtual IList<ITimestampedObjectMetadata>* getTrack() const override;
    virtual IUncompressedVideoFrame* getBestShotVideoFrame() const override;
    virtual ITimestampedObjectMetadata* getBestShotObjectMetadata() const override;

private:
    Ptr<IList<ITimestampedObjectMetadata>> m_track;
    Ptr<IUncompressedVideoFrame> m_bestShotVideoFrame;
    Ptr<ITimestampedObjectMetadata> m_bestShotObjectMetadata;

    std::vector<char> m_bestShotImageData;
    std::string m_bestShotImageDataFormat;

    std::string m_titleText;
    std::vector<char> m_titleImageData;
    std::string m_titleImageDataFormat;
};

} // namespace nx::sdk::analytics
