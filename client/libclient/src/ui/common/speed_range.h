#pragma once

struct QnSpeedRange
{
    qreal forward;
    qreal reverse;

    QnSpeedRange(qreal forward = 0.0, qreal reverse = 0.0);

    bool fuzzyEquals(const QnSpeedRange& other) const;

    QnSpeedRange expandedTo(QnSpeedRange& other) const;
    QnSpeedRange& expandTo(QnSpeedRange& other);

    QnSpeedRange boundedTo(QnSpeedRange& other) const;
    QnSpeedRange& boundTo(QnSpeedRange& other);
};
