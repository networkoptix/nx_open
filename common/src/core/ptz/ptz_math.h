#ifndef QN_PTZ_MATH_H
#define QN_PTZ_MATH_H

#include <QtCore/QtMath>

/**
 * \param mm35Equiv                 Width-based 35mm-equivalent focal length.
 * \returns                         Width-based FOV in degrees.
 */
inline qreal q35mmEquivToDegrees(qreal mm35Equiv) {
    return qRadiansToDegrees(std::atan((36.0 / 2.0) / mm35Equiv) * 2.0);
}

/**
 * \param degrees                   Width-based FOV in degrees.
 * \returns                         Width-based 35mm-equivalent focal length.
 */
inline qreal qDegreesTo35mmEquiv(qreal degrees) {
    return (36.0 / 2.0) / std::tan(qDegreesToRadians(degrees) / 2.0);
}

#endif // QN_PTZ_MATH_H
