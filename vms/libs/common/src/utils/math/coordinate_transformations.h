#ifndef QN_COORDINATE_TRANSFORMATIONS_H
#define QN_COORDINATE_TRANSFORMATIONS_H

#include <cmath> /* For std::sin, std::cos, std::atan2, ... */

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

template<class T>
struct QnPolarPoint {
    T r, alpha;
};

template<class T>
struct QnSphericalPoint {
    T r, phi, psi;
};


template<class T>
struct coord_type;

template<>
struct coord_type<QVector2D> {
    typedef float type;
};

template<>
struct coord_type<QVector3D> {
    typedef float type;
};

template<>
struct coord_type<QVector4D> {
    typedef float type;
};

template<>
struct coord_type<QPointF> {
    typedef qreal type;
};

template<class T> 
inline T polarToCartesian(typename coord_type<T>::type r, typename coord_type<T>::type alpha);

template<>
inline QVector2D polarToCartesian<QVector2D>(float r, float alpha) {
    return QVector2D(r * std::cos(alpha), r * std::sin(alpha));
}

template<>
inline QPointF polarToCartesian<QPointF>(qreal r, qreal alpha) {
    return QPointF(r * std::cos(alpha), r * std::sin(alpha));
}

template<class T>
inline QnPolarPoint<typename coord_type<T>::type> cartesianToPolar(const T &vector) {
    QnPolarPoint<typename coord_type<T>::type> result;
    result.r = std::sqrt(vector.x() * vector.x() + vector.y() * vector.y());
    result.alpha = std::atan2(vector.y(), vector.x());
    return result;
}


template<class T>
inline T sphericalToCartesian(typename coord_type<T>::type r, typename coord_type<T>::type phi, typename coord_type<T>::type psi);

template<>
inline QVector3D sphericalToCartesian(float r, float phi, float psi) {
    return QVector3D(r * std::cos(phi) * std::cos(psi), r * std::sin(phi) * std::cos(psi), r * std::sin(psi));
}


template<class T>
inline QnSphericalPoint<typename coord_type<T>::type> cartesianToSpherical(const T &vector) {
    QnSphericalPoint<typename coord_type<T>::type> result;
    result.r = std::sqrt(vector.x() * vector.x() + vector.y() * vector.y() + vector.z() * vector.z());
    result.phi = std::atan2(vector.y(), vector.x());
    result.psi = std::asin(vector.z() / result.r);
    return result;
}


#endif // QN_COORDINATE_TRANSFORMATIONS_H
