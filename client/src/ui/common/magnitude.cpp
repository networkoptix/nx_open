#include "magnitude.h"
#include <cassert>
#include <cmath> /* For std::abs & std::sqrt. */
#include <QPointF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVariant>
#include <QMetaType>
#include "protected_storage.h"

namespace {
    template<class T>
    class StandardMagnitudeCalculator: public MagnitudeCalculator {
    public:
        StandardMagnitudeCalculator(): MagnitudeCalculator(qRegisterMetaType<T>()) {}

    protected:
        virtual qreal calculateInternal(const void *value) override {
            return calculateMagnitude(*static_cast<const T *>(value));
        }
    };

    class ProtectedMagnitudeStorage: public ProtectedStorage<MagnitudeCalculator> {
    public:
        ProtectedMagnitudeStorage() {
            add(new StandardMagnitudeCalculator<float>());
            add(new StandardMagnitudeCalculator<double>());
            add(new StandardMagnitudeCalculator<QPointF>());
            add(new StandardMagnitudeCalculator<QVector2D>());
            add(new StandardMagnitudeCalculator<QVector3D>());
            add(new StandardMagnitudeCalculator<QVector4D>());
        }

        void add(MagnitudeCalculator *calculator) {
            set(calculator->type(), calculator);
        }
    };

    Q_GLOBAL_STATIC(ProtectedMagnitudeStorage, magnitudeStorage);

} // anonymous namespace

MagnitudeCalculator *MagnitudeCalculator::forType(int type) {
    return magnitudeStorage()->get(type);
}

void MagnitudeCalculator::registerForType(int type, MagnitudeCalculator *calculator) {
    magnitudeStorage()->set(type, calculator);
}

qreal MagnitudeCalculator::calculate(const QVariant &value) {
    assert(value.type() == m_type);

    return calculate(value.data());
}

qreal MagnitudeCalculator::calculate(const void *value) {
    assert(value != NULL);

    return calculateInternal(value);
}



qreal calculateMagnitude(float value) {
    return std::abs(value);
}

qreal calculateMagnitude(double value) {
    return std::abs(value);
}

qreal calculateMagnitude(const QPointF &value) {
    return std::sqrt(value.x() * value.x() + value.y() * value.y());
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
