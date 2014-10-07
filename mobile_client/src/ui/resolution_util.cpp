#include "resolution_util.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

namespace {
    const qreal referencePpi = 160.0;
}

QnResolutionUtil::DensityClass QnResolutionUtil::currentDensityClass() {
    static QList<qreal> standardMultipliers;
    if (standardMultipliers.isEmpty()) {
        standardMultipliers << densityMultiplier(Ldpi)
                            << densityMultiplier(Mdpi)
                            << densityMultiplier(Hdpi)
                            << densityMultiplier(Xhdpi)
                            << densityMultiplier(Xxhdpi)
                            << densityMultiplier(Xxxhdpi);
    }

    qreal ppi = QGuiApplication::primaryScreen()->physicalDotsPerInch() * QGuiApplication::primaryScreen()->devicePixelRatio();
    qreal multiplier = ppi / referencePpi;

    auto it = qLowerBound(standardMultipliers, multiplier);
    if (it == standardMultipliers.begin())
        return Ldpi;
    if (it == standardMultipliers.end())
        return Xxxhdpi;

    if (qAbs(multiplier - *it) < qAbs(multiplier - *(it - 1)))
        return static_cast<DensityClass>(it - standardMultipliers.begin());
    else
        return static_cast<DensityClass>(it - 1 - standardMultipliers.begin());
}

qreal QnResolutionUtil::densityMultiplier(QnResolutionUtil::DensityClass densityClass) {
    switch (densityClass) {
    case Ldpi: return 0.75;
    case Mdpi: return 1.0;
    case Hdpi: return 1.5;
    case Xhdpi: return 2.0;
    case Xxhdpi: return 3.0;
    case Xxxhdpi: return 4.0;
    }
    return 1.0;
}

QString QnResolutionUtil::densityName(QnResolutionUtil::DensityClass densityClass) {
    switch (densityClass) {
    case Ldpi: return lit("ldpi");
    case Mdpi: return lit("mdpi");
    case Hdpi: return lit("hdpi");
    case Xhdpi: return lit("xhdpi");
    case Xxhdpi: return lit("xxhdpi");
    case Xxxhdpi: return lit("xxxhdpi");
    }
    return QString();
}
