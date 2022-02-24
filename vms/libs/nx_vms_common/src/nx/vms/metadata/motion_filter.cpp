// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_filter.h"

namespace nx::vms::metadata {

MotionRecordMatcher::MotionRecordMatcher(const MotionFilter* filter):
    base_type(filter)
{
    const auto region = filter->region.isEmpty()
        ? QRect(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight) : filter->region;
    QnMetaDataV1::createMask(region, (char*) m_mask, &m_maskStart, &m_maskEnd);
}

const MotionFilter* MotionRecordMatcher::filter() const
{
    return static_cast<const MotionFilter*>(base_type::filter());
}

bool MotionRecordMatcher::matchRecord(
    int64_t /*timestampMs*/, const uint8_t* data, int /*recordSize*/) const
{
    return QnMetaDataV1::matchImage((quint64*)data, m_mask, m_maskStart, m_maskEnd);
}


} // namespace nx::vms::metadata
