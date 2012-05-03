#include "linear_combination.h"
#include <cassert>
#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QColor>
#include <QVariant>
#include <QMetaType>
#include "protected_storage.h"

namespace {
    template<class T>
    T linearCombineInternal(qreal a, const T &x, qreal b, const T &y) {
        return static_cast<T>(a * x + b * y);
    }

    template<class T>
    class StandardLinearCombinator: public LinearCombinator {
    public:
        StandardLinearCombinator(): LinearCombinator(qRegisterMetaType<T>()) { initZero(); }

    protected:
        virtual void calculateInternal(qreal a, const void *x, qreal b, const void *y, void *result) const override {
            *static_cast<T *>(result) = linearCombine(a, *static_cast<const T *>(x), b, *static_cast<const T *>(y));
        }
    };

    class NoopLinearCombinator: public LinearCombinator {
    public:
        NoopLinearCombinator(): LinearCombinator(0) { initZero(); }

    protected:
        virtual void calculateInternal(qreal, const void *, qreal, const void *, void *) const override {}
    };

    class Storage: public ProtectedStorage<LinearCombinator> {
    public:
        Storage() {
            add(new NoopLinearCombinator());
            add(new StandardLinearCombinator<int>());
            add(new StandardLinearCombinator<long>());
            add(new StandardLinearCombinator<long long>());
            add(new StandardLinearCombinator<float>());
            add(new StandardLinearCombinator<double>());
            add(new StandardLinearCombinator<QPointF>());
            add(new StandardLinearCombinator<QSizeF>());
            add(new StandardLinearCombinator<QRectF>());
            add(new StandardLinearCombinator<QVector2D>());
            add(new StandardLinearCombinator<QVector3D>());
            add(new StandardLinearCombinator<QVector4D>());
            add(new StandardLinearCombinator<QColor>());
        }

        void add(LinearCombinator *calculator) {
            set(calculator->type(), calculator);
        }
    };

    Q_GLOBAL_STATIC(Storage, storage);

} // anonymous namespace


LinearCombinator::LinearCombinator(int type): 
    m_type(type)
{}

LinearCombinator *LinearCombinator::forType(int type) {
    return storage()->get(type);
}

void LinearCombinator::registerCombinator(LinearCombinator *combinator) {
    combinator->initZero();
    storage()->set(combinator->type(), combinator);
}

QVariant LinearCombinator::combine(qreal a, const QVariant &x, qreal b, const QVariant &y) const {
    assert((x.userType() == m_type && y.userType() == m_type) || m_type == 0);

    QVariant result(m_type, static_cast<const void *>(NULL));
    calculateInternal(a, x.constData(), b, y.constData(), result.data());
    return result;
}

void LinearCombinator::combine(qreal a, const void *x, qreal b, const void *y, void *result) const {
    assert(x != NULL && y != NULL && result != NULL);

    calculateInternal(a, x, b, y, result);
}

QVariant LinearCombinator::combine(qreal a, const QVariant &x) const {
    return combine(a, x, 0.0, m_zero);
}

void LinearCombinator::combine(qreal a, const void *x, void *result) const {
    combine(a, x, 0.0, m_zero.constData(), result);
}

void LinearCombinator::initZero() {
    QVariant tmp(m_type, static_cast<const void *>(NULL));
    m_zero = combine(0.0, tmp, 0.0, tmp);
}

int linearCombine(qreal a, int x, qreal b, int y) {
    return linearCombineInternal(a, x, b, y);
}

long linearCombine(qreal a, long x, qreal b, long y) {
    return linearCombineInternal(a, x, b, y);
}

long long linearCombine(qreal a, long long x, qreal b, long long y) {
    return linearCombineInternal(a, x, b, y);
}

float linearCombine(qreal a, float x, qreal b, float y) {
    return linearCombineInternal(a, x, b, y);
}

double linearCombine(qreal a, double x, qreal b, double y) {
    return linearCombineInternal(a, x, b, y);
}

QPointF linearCombine(qreal a, const QPointF &x, qreal b, const QPointF &y) {
    return linearCombineInternal(a, x, b, y);
}

QSizeF linearCombine(qreal a, const QSizeF &x, qreal b, const QSizeF &y) {
    return linearCombineInternal(a, x, b, y);
}

QVector2D linearCombine(qreal a, const QVector2D &x, qreal b, const QVector2D &y) {
    return linearCombineInternal(a, x, b, y);
}

QVector3D linearCombine(qreal a, const QVector3D &x, qreal b, const QVector3D &y) {
    return linearCombineInternal(a, x, b, y);
}

QVector4D linearCombine(qreal a, const QVector4D &x, qreal b, const QVector4D &y) {
    return linearCombineInternal(a, x, b, y);
}

QColor linearCombine(qreal a, const QColor &x, qreal b, const QColor &y) {
    return QColor(
        qBound(0, linearCombine(a, x.red(),   b, y.red()),   255),
        qBound(0, linearCombine(a, x.green(), b, y.green()), 255),
        qBound(0, linearCombine(a, x.blue(),  b, y.blue()),  255),
        qBound(0, linearCombine(a, x.alpha(), b, y.alpha()), 255)
    );
}

QRectF linearCombine(qreal a, const QRectF &x, qreal b, const QRectF &y) {
    return QRectF(
        linearCombine(a, x.topLeft(),   b, y.topLeft()),
        linearCombine(a, x.size(),      b, y.size())
    );
}

