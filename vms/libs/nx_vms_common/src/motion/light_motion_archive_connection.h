// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_motion_archive.h"

class QnLightMotionArchiveConnection: public QnAbstractMotionArchiveConnection
{
public:
    QnLightMotionArchiveConnection(const QnMetaDataLightVector& data, int channel);
    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override;

private:
    const QnMetaDataLightVector m_motionData;
    int m_channel;
    QnMetaDataV1Ptr m_lastResult;
};
