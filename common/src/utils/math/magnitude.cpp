#include "magnitude.h"

#include <cassert>
#include <cmath> /* For std::abs & std::sqrt. */

#include <QtCore/QPointF>
#include <QtCore/QSizeF>
#include <QtCore/QRectF>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>
#include <QtGui/QColor>

#include <nx/utils/synchronized_flat_storage.h>
#include <nx/utils/log/assert.h>

namespace {
    template<class T>
    class StandardMagnitudeCalculator: public MagnitudeCalculator {
    public:
        StandardMagnitudeCalculator(): MagnitudeCalculator(qRegisterMetaType<T>()) {}

    protected:
        virtual qreal calculateInternal(const void *value) const override {
            return calculateMagnitude(*static_cast<const T *>(value));
        }
    };

    class NoopMagnitudeCalculator: public MagnitudeCalculator {
    public:
        NoopMagnitudeCalculator(): MagnitudeCalculator(QMetaType::UnknownType) {}

    private:
        virtual qreal calculateInternal(const void *) const override {
            return 0.0;
        }
    };

    class Storage: public QnSynchronizedFlatStorage<unsigned int, MagnitudeCalculator *> {
        typedef QnSynchronizedFlatStorage<unsigned int, MagnitudeCalculator *> base_type;
    public:
        Storage() {
            insert(new NoopMagnitudeCalculator());
            insert(new StandardMagnitudeCalculator<int>());
            insert(new StandardMagnitudeCalculator<long>());
            insert(new StandardMagnitudeCalculator<long long>());
            insert(new StandardMagnitudeCalculator<float>());
            insert(new StandardMagnitudeCalculator<double>());
            insert(new StandardMagnitudeCalculator<QPoint>());
            insert(new StandardMagnitudeCalculator<QPointF>());
            insert(new StandardMagnitudeCalculator<QSizeF>());
            insert(new StandardMagnitudeCalculator<QRectF>());
            insert(new StandardMagnitudeCalculator<QVector2D>());
            insert(new StandardMagnitudeCalculator<QVector3D>());
            insert(new StandardMagnitudeCalculator<QVector4D>());
            insert(new StandardMagnitudeCalculator<QColor>());
        }

        using base_type::insert;

        void insert(MagnitudeCalculator *calculator) {
            insert(calculator->type(), calculator);
        }
    };

    Q_GLOBAL_STATIC(Storage, storage);

} // anonymous namespace

MagnitudeCalculator *MagnitudeCalculator::forType(int type) {
    return storage()->value(type);
}

void MagnitudeCalculator::registerCalculator(MagnitudeCalculator *calculator) {
    storage()->insert(calculator);
}

qreal MagnitudeCalculator::calculate(const QVariant &value) const {
    NX_ASSERT(value.userType() == m_type || m_type == 0);

    return calculate(value.constData());
}

qreal MagnitudeCalculator::calculate(const void *value) const {
    NX_ASSERT(value != NULL);

    return calculateInternal(value);
}

qreal calculateMagnitude(int value) {
    return std::abs(value);
}

qreal calculateMagnitude(long value) {
    return std::abs(value);
}

qreal calculateMagnitude(long long value) {
#if (defined(_MSC_VER) && _MSC_VER < 1600) || (defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5))) /* VC++ 2008 and earlier */
    return qAbs(value);
#else
    return std::abs(value);
#endif
}

qreal calculateMagnitude(float value) {
    return std::abs(value);
}

qreal calculateMagnitude(double value) {
    return std::abs(value);
}

qreal calculateMagnitude(const QPoint &value) {
    return calculateMagnitude(QPointF(value));
}

qreal calculateMagnitude(const QPointF &value) {
    return std::sqrt(value.x() * value.x() + value.y() * value.y());
}

qreal calculateMagnitude(const QSize &value) {
    return calculateMagnitude(QSizeF(value));
}

qreal calculateMagnitude(const QSizeF &value) {
    return std::sqrt(value.width() * value.width() + value.height() * value.height());
}

qreal calculateMagnitude(const QVector2D &value) {
    return value.length();
}

qreal calculateMagnitude(const QVector3D &value) {
    return value.length();
}

qreal calculateMagnitude(const QVector4D &value) {
    return value.length();
}

qreal calculateMagnitude(const QColor &value) {
    return calculateMagnitude(QVector4D(value.redF(), value.greenF(), value.blueF(), value.alphaF()));
}

qreal calculateMagnitude(const QRectF &value) {
    return calculateMagnitude(QVector4D(value.top(), value.left(), value.width(), value.height()));
}
