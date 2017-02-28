#pragma once

struct QnSpeedRange
{
    qreal forward; //< maximum forward speed (>= 0)
    qreal reverse; //< maximum reverse speed (>= 0)

    QnSpeedRange(qreal forward = 0.0, qreal reverse = 0.0);

    bool fuzzyEquals(const QnSpeedRange& other) const;

    QnSpeedRange expandedTo(QnSpeedRange& other) const;
    QnSpeedRange& expandTo(QnSpeedRange& other);

    QnSpeedRange limitedBy(QnSpeedRange& other) const;
    QnSpeedRange& limitBy(QnSpeedRange& other);
};
