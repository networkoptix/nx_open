#include "fuzzy.h"

#include <cmath>

#include <QtCore/QtNumeric>

float qFuzzyFloor(float value) {
    float result = std::floor(value);

    if(qFuzzyIsNull(value - result - 1.0f)) {
        return result + 1.0f;
    } else {
        return result;
    }
}

double qFuzzyFloor(double value) {
    double result = std::floor(value);

    if(qFuzzyIsNull(value - result - 1.0)) {
        return result + 1.0;
    } else {
        return result;
    }
}

float qFuzzyCeil(float value) {
    float result = std::ceil(value);

    if(qFuzzyIsNull(result - value - 1.0f)) {
        return result - 1.0f;
    } else {
        return result;
    }
}

double qFuzzyCeil(double value) {
    double result = std::ceil(value);

    if(qFuzzyIsNull(result - value - 1.0)) {
        return result - 1.0;
    } else {
        return result;
    }
}

bool qFuzzyContains(const QRectF &rect, const QPointF &point) {
    return 
        qFuzzyBetween(rect.left(), point.x(), rect.right()) &&
        qFuzzyBetween(rect.top(), point.y(), rect.bottom());
}


