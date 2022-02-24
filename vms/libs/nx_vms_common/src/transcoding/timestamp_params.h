// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

struct QnTimeStampParams
{
    bool enabled = false;
    Qt::Corner corner = Qt::BottomRightCorner;
    qint64 displayOffset = 0;
    qint64 timeMs = 0;
};
