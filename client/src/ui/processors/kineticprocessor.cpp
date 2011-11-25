#include "kineticprocessor.h"
#include <cmath> /* For std::abs & std::sqrt. */
#include <QPointF>

namespace detail {
    qreal calculateMagnitude(float value) {
        return std::abs(value);
    }

    qreal calculateMagnitude(double value) {
        return std::abs(value);
    }

    qreal calculateMagnitude(const QPointF &point) {
        return std::sqrt(point.x() * point.x() + point.y() * point.y());
    }

}

