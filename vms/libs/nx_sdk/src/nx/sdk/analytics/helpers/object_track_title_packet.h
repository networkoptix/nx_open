// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/i_object_track_title_packet.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::analytics {

class ObjectTrackTitlePacket: public RefCountable<IObjectTrackTitlePacket>
{
public:
    ObjectTrackTitlePacket(Uuid trackId = Uuid());

    virtual const char* text() const override;
    virtual const char* imageUrl() const override;
    virtual const char* imageData() const override;
    virtual int imageDataSize() const override;
    virtual const char* imageDataFormat() const override;

    /** See IObjectTrackTitlePacket::trackId(). */
    void setTrackId(const Uuid& trackId);

    /** See IObjectTrackTitlePacket::text(). */
    void setText(std::string text);

    /** See IObjectTrackTitlePacket::imageUrl(). */
    void setImageUrl(std::string imageUrl);

    /** See IObjectTrackTitlePacket::imageData(). */
    void setImageData(std::vector<char> imageData);

    /** See IObjectTrackTitlePacket::imageDataFormat(). */
    void setImageDataFormat(std::string imageDataFormat);

    /**
     * Stores image binary data - calls setImageDataFormat() and setImageData().
     * @param imageDataFormat See IObjectTrackTitlePacket::imageDataFormat().
     */
    void setImage(std::string imageDataFormat, std::vector<char> imageData);

protected:
    virtual void getTrackId(Uuid* outValue) const override;

private:
    Uuid m_trackId;
    std::string m_text;

    std::string m_imageUrl;
    std::vector<char> m_imageData;
    std::string m_imageDataFormat;
};

} // namespace nx::sdk::analytics