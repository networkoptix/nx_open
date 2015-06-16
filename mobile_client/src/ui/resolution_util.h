#ifndef RESOLUTION_UTIL_H
#define RESOLUTION_UTIL_H

#include <QtCore/Qt>

#include <utils/common/singleton.h>

class QnResolutionUtil : public Singleton<QnResolutionUtil> {
public:

    enum DensityClass {
        Ldpi,
        Mdpi,
        Hdpi,
        Xhdpi,
        Xxhdpi,
        Xxxhdpi
    };

    static QString densityName(DensityClass densityClass);
    static qreal densityMultiplier(DensityClass densityClass);

    QnResolutionUtil();

    DensityClass densityClass() const;
    QString densityName() const;
    qreal densityMultiplier() const;

    int dp(qreal dpix) const;
    int sp(qreal dpix) const;

private:
    DensityClass currentDensityClass() const;

private:
    DensityClass m_densityClass;
    qreal m_multiplier;
};

#endif // RESOLUTION_UTIL_H
