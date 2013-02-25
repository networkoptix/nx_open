#include "fuzzy.h"
#include <cmath>


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
        qFuzzyBetween(point.x(), rect.left(), rect.right()) &&
        qFuzzyBetween(point.y(), rect.top(), rect.bottom());
}


