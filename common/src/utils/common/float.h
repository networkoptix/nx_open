#ifndef QN_FLOAT_H
#define QN_FLOAT_H

#ifdef _MSC_VER
#   include <cfloat>

bool qnIsFinite(double value) {
    switch(_fpclass(value)) {
    case _FPCLASS_SNAN:
    case _FPCLASS_QNAN:
    case _FPCLASS_NINF:
    case _FPCLASS_PINF:
        return false;
    default:
        return true;
    }
}

#else
#   include <cmath>

bool qnIsFinite(double value) {
    switch(fpclassify(value)) {
    case FP_NAN:
    case FP_INFINITE:
        return false;
    default:
        return true;
    }
}

#endif

#endif // QN_FLOAT_H