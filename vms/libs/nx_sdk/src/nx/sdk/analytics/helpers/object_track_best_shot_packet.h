// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::analytics {

class ObjectTrackBestShotPacket: public RefCountable<IObjectTrackBestShotPacket>
{
public:
    ObjectTrackBestShotPacket(Uuid trackId = Uuid(), int64_t timestampUs = -1, Rect boundingBox = Rect());

    virtual Flags flags() const override;
    virtual int64_t timestampUs() const override;

    virtual const char* imageUrl() const override;
    virtual const char* imageData() const override;
    virtual int imageDataSize() const override;
    virtual const char* imageDataFormat() const override;

    /** See IObjectTrackBestShotPacket0::trackId(). */
    void setTrackId(const Uuid& trackId);

    /** See IObjectTrackBestShotPacket::flags(). */
    void setFlags(Flags flags);

    /** See IDataPacket::timestampUs(). */
    void setTimestampUs(int64_t timestampUs);

    /** See IObjectTrackBestShotPacket0::boundingBox(). */
    void setBoundingBox(const Rect& boundingBox);

    /** See IObjectTrackBestShotPacket1::imageUrl(). */
    void setImageUrl(std::string imageUrl);

    /** See IObjectTrackBestShotPacket1::imageData(). */
    void setImageData(std::vector<char> imageData);

    /** See IObjectTrackBestShotPacket1::imageDataFormat(). */
    void setImageDataFormat(std::string imageDataFormat);

    /**
     * Stores image binary data - calls setImageDataFormat() and setImageData().
     * @param imageDataFormat See IObjectTrackBestShotPacket1::imageDataFormat().
     */
    void setImage(std::string imageDataFormat, std::vector<char> imageData);

protected:
    virtual const IAttribute* getAttribute(int index) const override;

public:
    virtual int attributeCount() const override;

    void addAttribute(Ptr<Attribute> attribute);
    void addAttributes(const std::vector<Ptr<Attribute>>& value);

protected:
    virtual void getTrackId(Uuid* outValue) const override;
    virtual void getBoundingBox(Rect* outValue) const override;

private:
    Uuid m_trackId;
    Flags m_flags = Flags::none;
    int64_t m_timestampUs = -1;
    Rect m_boundingBox;

    std::string m_imageUrl;
    std::vector<char> m_imageData;
    std::string m_imageDataFormat;

    std::vector<Ptr<Attribute>> m_attributes;
};

} // namespace nx::sdk::analytics
