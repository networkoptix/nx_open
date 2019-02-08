#pragma once

struct QnSpeedRange
{
    qreal forward; //< maximum forward speed (>= 0)
    qreal backward; //< maximum backward speed (>= 0)

    QnSpeedRange(qreal forward = 0.0, qreal backward = 0.0);

    bool fuzzyEquals(const QnSpeedRange& other) const;

    QnSpeedRange expandedTo(QnSpeedRange& other) const;
    QnSpeedRange& expandTo(QnSpeedRange& other);

    QnSpeedRange limitedBy(QnSpeedRange& other) const;
    QnSpeedRange& limitBy(QnSpeedRange& other);

    /* Binds signed speed to [-backward, forward] range. */
    qreal boundSpeed(qreal speed) const;
};
