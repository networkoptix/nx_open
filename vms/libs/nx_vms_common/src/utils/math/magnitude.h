// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_MAGNITUDE_H
#define QN_MAGNITUDE_H

#include <nx/utils/log/assert.h>

#include <typeinfo>

class QPoint;
class QPointF;
class QSize;
class QSizeF;
class QRectF;
class QVector2D;
class QVector3D;
class QVector4D;
class QColor;
class QVariant;

qreal calculateMagnitude(int value);
qreal calculateMagnitude(long value);
qreal calculateMagnitude(long long value);
qreal calculateMagnitude(float value);
qreal calculateMagnitude(double value);
qreal calculateMagnitude(const QPoint &value);
qreal calculateMagnitude(const QPointF &value);
qreal calculateMagnitude(const QSize &value);
qreal calculateMagnitude(const QSizeF &value);
qreal calculateMagnitude(const QVector2D &value);
qreal calculateMagnitude(const QVector3D &value);
qreal calculateMagnitude(const QVector4D &value);
qreal calculateMagnitude(const QColor &value);
qreal calculateMagnitude(const QRectF &value); /* QRectF is treated as a 4D vector here. */

template<class T>
qreal calculateMagnitude(const T&, ...) {
    NX_ASSERT(false, "calculateMagnitude function is not implemented for type '%1'.", typeid(T).name());
    return 0.0;
}

template<class T>
class TypedMagnitudeCalculator;

class NX_VMS_COMMON_API MagnitudeCalculator
{
public:
    /**
     * \param type                      <tt>QMetaType::Type</tt> to get magnitude calculator for.
     *                                  Pass zero to get no-op calculator.
     * \returns                         Magnitude calculator for the given type, or NULL if none.
     *
     * \note                            This function is thread-safe.
     */
    static MagnitudeCalculator *forType(QMetaType type);

    /**
     * \tparam T                        Type to get magnitude calculator for.
     * \returns                         Magnitude calculator for the given type, or NULL if none.
     *
     * \note                            This function is thread-safe.
     */
    template<class T>
    static TypedMagnitudeCalculator<T> *forType() {
        return forType(QMetaType::fromType<T>())->template typed<T>();
    }

    /**
     * \param calculator                New magnitude calculator to register.
     *
     * \note                            This function is thread-safe.
     */
    static void registerCalculator(MagnitudeCalculator *calculator);

    /**
     * Constructor.
     *
     * \param type                      <tt>QMetaType::Type</tt> for this magnitude calculator.
     */
    MagnitudeCalculator(QMetaType type): m_type(type) {}

    /**
     * Virtual destructor.
     */
    virtual ~MagnitudeCalculator() {}

    /**
     * \returns                         <tt>QMetaType::Type</tt> of this magnitude calculator.
     */
    QMetaType type() const { return m_type; }

    /**
     * Note that this function will NX_ASSERT if the type of the supplied variant
     * does not match the type of this magnitude calculator.
     *
     * \param value                     Value to calculate magnitude for.
     * \returns                         Magnitude of the given value.
     */
    qreal calculate(const QVariant &value) const;

    qreal calculate(const void *value) const;

    template<class T>
    inline const TypedMagnitudeCalculator<T> *typed() const;

    template<class T>
    TypedMagnitudeCalculator<T> *typed() {
        return const_cast<TypedMagnitudeCalculator<T> *>(static_cast<const MagnitudeCalculator *>(this)->typed<T>());
    }

protected:
    /**
     * \param value                     Value to calculate magnitude for.
     * \returns                         Magnitude of the given value.
     */
    virtual qreal calculateInternal(const void *value) const = 0;

private:
    QMetaType m_type;
};


template<class T>
class TypedMagnitudeCalculator: public MagnitudeCalculator {
    typedef MagnitudeCalculator base_type;
public:
    TypedMagnitudeCalculator(): base_type(QMetaType::fromType<T>()) {}

    using base_type::calculate;

    qreal calculate(const T &value) const {
        return base_type::calculate(static_cast<const void *>(&value));
    }
};


template<class T>
const TypedMagnitudeCalculator<T> *MagnitudeCalculator::typed() const {
    if(QMetaType::fromType<T>() == m_type) {
        return static_cast<const TypedMagnitudeCalculator<T> *>(this);
    } else {
        return NULL;
    }
}


#endif // QN_MAGNITUDE_H
