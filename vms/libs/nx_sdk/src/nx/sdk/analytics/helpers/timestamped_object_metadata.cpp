#include "timestamped_object_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

TimestampedObjectMetadata::TimestampedObjectMetadata():
    m_objectMetadata(nx::sdk::makePtr<ObjectMetadata>())
{
}

const char* TimestampedObjectMetadata::typeId() const
{
    return m_objectMetadata->typeId();
}

float TimestampedObjectMetadata::confidence() const
{
    return m_objectMetadata->confidence();
}

Uuid TimestampedObjectMetadata::id() const
{
    return m_objectMetadata->id();
}

const char* TimestampedObjectMetadata::subtype() const
{
    return m_objectMetadata->subtype();
}

const IAttribute* TimestampedObjectMetadata::attribute(int index) const
{
    return m_objectMetadata->attribute(index);
}

int TimestampedObjectMetadata::attributeCount() const
{
    return m_objectMetadata->attributeCount();
}

const char* TimestampedObjectMetadata::auxiliaryData() const
{
    return m_objectMetadata->auxiliaryData();
}

Rect TimestampedObjectMetadata::boundingBox() const
{
    return m_objectMetadata->boundingBox();
}

int64_t TimestampedObjectMetadata::timestampUs() const
{
    return m_timestampUs;
}

void TimestampedObjectMetadata::setTypeId(std::string typeId)
{
    m_objectMetadata->setTypeId(std::move(typeId));
}

void TimestampedObjectMetadata::setConfidence(float confidence)
{
    m_objectMetadata->setConfidence(confidence);
}

void TimestampedObjectMetadata::setId(const Uuid& value)
{
    m_objectMetadata->setId(std::move(value));
}

void TimestampedObjectMetadata::setSubtype(const std::string& value)
{
    m_objectMetadata->setSubtype(std::move(value));
}

void TimestampedObjectMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
{
    m_objectMetadata->addAttribute(std::move(attribute));
}

void TimestampedObjectMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    m_objectMetadata->addAttributes(value);
}

void TimestampedObjectMetadata::setAuxiliaryData(std::string value)
{
    m_objectMetadata->setAuxiliaryData(std::move(value));
}

void TimestampedObjectMetadata::setBoundingBox(const Rect& rect)
{
    m_objectMetadata->setBoundingBox(std::move(rect));
}

void TimestampedObjectMetadata::setTimestampUs(int64_t timestamp)
{
    m_timestampUs = timestamp;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
