#ifndef QN_MAGNITUDE_H
#define QN_MAGNITUDE_H

#include <typeinfo>
#include <utils/common/warnings.h>

class QPointF;
class QSizeF;
class QRectF;
class QVector2D;
class QVector3D;
class QVector4D;
class QColor;
class QVariant;

qreal calculateMagnitude(int value);
qreal calculateMagnitude(float value);
qreal calculateMagnitude(double value);
qreal calculateMagnitude(const QPointF &value);
qreal calculateMagnitude(const QSizeF &value);
qreal calculateMagnitude(const QVector2D &value);
qreal calculateMagnitude(const QVector3D &value);
qreal calculateMagnitude(const QVector4D &value);
qreal calculateMagnitude(const QColor &value);
qreal calculateMagnitude(const QRectF &value); /* QRectF is treated as a 4D vector here. */

template<class T>
qreal calculateMagnitude(const T &value, ...) {
    qnWarning("calculateMagnitude function is not implemented for type '%1'.", typeid(T).name());
    return 0.0;
}

class MagnitudeCalculator {
public:
    /**
     * This function is thread-safe.
     *
     * \param type                      <tt>QMetaType::Type</tt> to get magnitude calculator for. 
     *                                  Pass zero to get no-op calculator.
     * \returns                         Magnitude calculator for the given type, or NULL if none.
     */
    static MagnitudeCalculator *forType(int type);

    /**
     * This function is thread-safe.
     *
     * \param calculator                New magnitude calculator to register.
     */
    static void registerCalculator(MagnitudeCalculator *calculator);

    /**
     * Constructor.
     * 
     * \param type                      <tt>QMetaType::Type</tt> for this magnitude calculator.
     */
    MagnitudeCalculator(int type): m_type(type) {}

    /**
     * \returns                         <tt>QMetaType::Type</tt> of this magnitude calculator.
     */
    int type() const {
        return m_type;
    }

    /**
     * Note that this function will assert if the type of the supplied variant
     * does not match the type of this magnitude calculator.
     *
     * \param value                     Value to calculate magnitude for.
     * \returns                         Magnitude of the given value.
     */
    qreal calculate(const QVariant &value) const;

    qreal calculate(const void *value) const;

protected:
    /**
     * \param value                     Value to calculate magnitude for.
     * \returns                         Magnitude of the given value.
     */
    virtual qreal calculateInternal(const void *value) const = 0;

private:
    int m_type;
};

#endif // QN_MAGNITUDE_H
