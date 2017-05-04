#pragma once

struct QnTimeStampParams
{
    bool enabled = false;
    Qt::Corner corner = Qt::BottomRightCorner;
    qint64 displayOffset = 0;
    qint64 timeMs = 0;
};
