#ifndef QN_RECT_ANIMATOR_H
#define QN_RECT_ANIMATOR_H

#include "variant_animator.h"

/**
 * When animating <tt>QRectF</tt> properties, single speed value has little
 * sense. This class introduces new speed values that can be
 * adjusted to change transition times.
 */
class RectAnimator: public VariantAnimator {
    Q_OBJECT;

    typedef VariantAnimator base_type;

public:
    RectAnimator(QObject *parent = NULL);

    virtual ~RectAnimator();

    /**
     * \returns                         Scaling speed, scaling factor per second.
     */
    qreal scalingSpeed() const;

    /**
     * \returns                         Relative part of the movement speed, 
     *                                  in rects per second.
     */
    qreal relativeMovementSpeed() const {
        return m_relativeMovementSpeed;
    }

    /**
     * \returns                         Absolute part of the movement speed,
     *                                  in rectangle's space coordinates per second.
     */
    qreal absoluteMovementSpeed() const {
        return m_absoluteMovementSpeed;
    }

    /**
     * \param scalingSpeed              New scaling speed, scaling factor per second.
     */
    void setScalingSpeed(qreal scalingSpeed);

    /**
     * \param relativeMovementSpeed     New relative part of the movement speed, 
     *                                  in rects per second.
     */
    void setRelativeMovementSpeed(qreal relativeMovementSpeed);

    /**
     * \param absoluteMovementSpeed     New absolute part of the movement speed, 
     *                                  in rectangle's space coordinates per second.
     */
    void setAbsoluteMovementSpeed(qreal absoluteMovementSpeed);

protected:
    virtual void updateInternalType(int newType) override;

    virtual int estimatedDuration(const QVariant &from, const QVariant &to) const override;

private:
    qreal m_logScalingSpeed;
    qreal m_relativeMovementSpeed;
    qreal m_absoluteMovementSpeed;
};

#endif // QN_RECT_ANIMATOR_H
