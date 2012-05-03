#include "magnitude.h"
#include <cassert>
#include <cmath> /* For std::abs & std::sqrt. */
#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVariant>
#include <QColor>
#include <QMetaType>
#include "protected_storage.h"

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
        NoopMagnitudeCalculator(): MagnitudeCalculator(0) {}

    private:
        virtual qreal calculateInternal(const void *) const override {
            return 0.0;
        }
    };

    class Storage: public ProtectedStorage<MagnitudeCalculator> {
    public:
        Storage() {
            add(new NoopMagnitudeCalculator());
            add(new StandardMagnitudeCalculator<int>());
            add(new StandardMagnitudeCalculator<long>());
            add(new StandardMagnitudeCalculator<long long>());
            add(new StandardMagnitudeCalculator<float>());
            add(new StandardMagnitudeCalculator<double>());
            add(new StandardMagnitudeCalculator<QPoint>());
            add(new StandardMagnitudeCalculator<QPointF>());
            add(new StandardMagnitudeCalculator<QSizeF>());
            add(new StandardMagnitudeCalculator<QRectF>());
            add(new StandardMagnitudeCalculator<QVector2D>());
            add(new StandardMagnitudeCalculator<QVector3D>());
            add(new StandardMagnitudeCalculator<QVector4D>());
            add(new StandardMagnitudeCalculator<QColor>());
        }

        void add(MagnitudeCalculator *calculator) {
            set(calculator->type(), calculator);
        }
    };

    Q_GLOBAL_STATIC(Storage, storage);

} // anonymous namespace

MagnitudeCalculator *MagnitudeCalculator::forType(int type) {
    return storage()->get(type);
}

void MagnitudeCalculator::registerCalculator(MagnitudeCalculator *calculator) {
    storage()->set(calculator->type(), calculator);
}

qreal MagnitudeCalculator::calculate(const QVariant &value) const {
    assert(value.userType() == m_type || m_type == 0);

    return calculate(value.constData());
}

qreal MagnitudeCalculator::calculate(const void *value) const {
    assert(value != NULL);

    return calculateInternal(value);
}

qreal calculateMagnitude(int value) {
    return std::abs(value);
}

qreal calculateMagnitude(long value) {
    return std::abs(value);
}

qreal calculateMagnitude(long long value) {
    return std::abs(value);
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
