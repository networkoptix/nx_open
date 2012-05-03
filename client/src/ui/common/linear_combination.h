#ifndef QN_LINEAR_COMBINATION_H
#define QN_LINEAR_COMBINATION_H

#include <typeinfo>
#include <utils/common/warnings.h>
#include <QVariant>

class QPointF;
class QSizeF;
class QRectF;
class QVector2D;
class QVector3D;
class QVector4D;
class QColor;

int linearCombine(qreal a, int x, qreal b, int y);
long linearCombine(qreal a, long x, qreal b, long y);
long long linearCombine(qreal a, long long x, qreal b, long long y);
float linearCombine(qreal a, float x, qreal b, float y);
double linearCombine(qreal a, double x, qreal b, double y);
QPointF linearCombine(qreal a, const QPointF &x, qreal b, const QPointF &y);
QSizeF linearCombine(qreal a, const QSizeF &x, qreal b, const QSizeF &y);
QRectF linearCombine(qreal a, const QRectF &x, qreal b, const QRectF &y);
QVector2D linearCombine(qreal a, const QVector2D &x, qreal b, const QVector2D &y);
QVector3D linearCombine(qreal a, const QVector3D &x, qreal b, const QVector3D &y);
QVector4D linearCombine(qreal a, const QVector4D &x, qreal b, const QVector4D &y);
QColor linearCombine(qreal a, const QColor &x, qreal b, const QColor &y);

template<class T>
T linearCombine(qreal a, const T &x, qreal b, const T &y) {
    qnWarning("linearCombine function is not implemented for type '%1'.", typeid(T).name());
    return T();
}


class LinearCombinator {
public:
    /**
     * \param type                      <tt>QMetaType::Type</tt> to get linear combinator for. 
     *                                  Pass zero to get a no-op combinator.
     * \returns                         Linear combinator for the given type, or NULL if none.
     * 
     * \note                            This function is thread-safe.
     */
    static LinearCombinator *forType(int type);

    /**
     * \param combinator                Linear combinator to register.
     * 
     * \note                            This function is thread-safe.
     */
    static void registerCombinator(LinearCombinator *combinator);

    /**
     * Constructor.
     * 
     * \param type                      <tt>QMetaType::Type</tt> for this linear combinator.
     */
    LinearCombinator(int type);

    /**
     * Virtual destructor. 
     */
    virtual ~LinearCombinator() {}

    /**
     * \returns                         <tt>QMetaType::Type</tt> of this linear combinator.
     */
    int type() const {
        return m_type;
    }

    QVariant combine(qreal a, const QVariant &x, qreal b, const QVariant &y) const;

    void combine(qreal a, const void *x, qreal b, const void *y, void *result) const;

    QVariant combine(qreal a, const QVariant &x) const;

    void combine(qreal a, const void *x, void *result) const;

    QVariant zero() const {
        return m_zero;
    }

protected:
    /**
     * \param value                     Value to combine magnitude for.
     * \returns                         Magnitude of the given value.
     */
    virtual void calculateInternal(qreal a, const void *x, qreal b, const void *y, void *result) const = 0;

    void initZero();

private:
    int m_type;
    QVariant m_zero;
};

#endif // QN_LINEAR_COMBINATION_H
