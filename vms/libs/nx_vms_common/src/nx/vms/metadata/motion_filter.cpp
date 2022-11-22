// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_filter.h"

namespace nx::vms::metadata {

MotionRecordMatcher::MotionRecordMatcher(const MotionFilter* filter):
    base_type(filter)
{
    // Optimization for whole frame match.
    if (filter->region.isEmpty())
        m_wholeFrame = true; //< According to the API empty region means whole frame.
    for (const auto& rect: filter->region)
    {
        if (rect.width() == Qn::kMotionGridWidth && rect.height() == Qn::kMotionGridHeight)
        {
            m_wholeFrame = true;
            break;
        }
    }

    if (!m_wholeFrame)
        QnMetaDataV1::createMask(filter->region, (char*) m_mask, &m_maskStart, &m_maskEnd);
}

const MotionFilter* MotionRecordMatcher::filter() const
{
    return static_cast<const MotionFilter*>(base_type::filter());
}

bool MotionRecordMatcher::matchRecord(
    int64_t /*timestampMs*/, const uint8_t* data, int /*recordSize*/) const
{
    if (m_wholeFrame)
        return true;
    return QnMetaDataV1::matchImage((quint64*)data, m_mask, m_maskStart, m_maskEnd);
}


} // namespace nx::vms::metadata
