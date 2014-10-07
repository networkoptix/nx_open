#ifndef RESOLUTION_UTIL_H
#define RESOLUTION_UTIL_H

#include <QtCore/Qt>

class QnResolutionUtil {
public:

    enum DensityClass {
        Ldpi,
        Mdpi,
        Hdpi,
        Xhdpi,
        Xxhdpi,
        Xxxhdpi
    };

    static DensityClass currentDensityClass();

    static qreal densityMultiplier(DensityClass densityClass);
    static QString densityName(DensityClass densityClass);

private:
};

#endif // RESOLUTION_UTIL_H
