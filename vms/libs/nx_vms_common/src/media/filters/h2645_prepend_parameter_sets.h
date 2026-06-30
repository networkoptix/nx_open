// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_media_data_filter.h"

/**
 * Prepends parameter sets to each H.264/H.265 keyframe if they are absent:
 * - H.264: SPS (type 7) and PPS (type 8) before IDR frames (type 5).
 *   NAL unit types: ITU-T H.264, Section 7.4.1, Table 7-1.
 *   https://www.itu.int/rec/T-REC-H.264/en
 * - H.265: VPS (type 32), SPS (type 33) and PPS (type 34) before IRAP frames (types 16-23).
 *   NAL unit types: ITU-T H.265, Section 7.4.2, Table 7-1.
 *   https://www.itu.int/rec/T-REC-H.265/en
 * Supports both Annex B (start-code) and MP4 (length-prefixed) input formats.
 */
class NX_VMS_COMMON_API H2645PrependParameterSets: public AbstractMediaDataFilter
{
public:
    virtual QnConstAbstractDataPacketPtr processData(
        const QnConstAbstractDataPacketPtr& data) override;
};
