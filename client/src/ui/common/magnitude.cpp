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

    return calculate(value.data());
}

qreal MagnitudeCalculator::calculate(const void *value) const {
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
