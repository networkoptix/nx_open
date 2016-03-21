#include "resolution_util.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#ifdef Q_OS_ANDROID
#include <QtAndroidExtras/QAndroidJniObject>
#endif

namespace {
    const qreal referencePpi = 160.0;

    qreal densityMultiplier(qreal *pixelRatio = nullptr) {
#if defined(Q_OS_ANDROID)
        QAndroidJniObject qtActivity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
        QAndroidJniObject resources = qtActivity.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
        QAndroidJniObject displayMetrics = resources.callObjectMethod("getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
        qreal ppi = displayMetrics.getField<int>("densityDpi");
        if (pixelRatio)
            *pixelRatio = 1.0;
#elif defined(Q_OS_IOS)
        qreal ppi = QGuiApplication::primaryScreen()->physicalDotsPerInch();
        if (pixelRatio)
            *pixelRatio = QGuiApplication::primaryScreen()->devicePixelRatio();
#else
        qreal ppi = QGuiApplication::primaryScreen()->physicalDotsPerInch() * QGuiApplication::primaryScreen()->devicePixelRatio();
        if (pixelRatio)
            *pixelRatio = 1.0;
#endif
        return ppi / referencePpi;
    }

    QnResolutionUtil::DensityClass densityClass(qreal multiplier) {
        static QList<qreal> standardMultipliers;
        if (standardMultipliers.isEmpty()) {
            standardMultipliers << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Ldpi)
                                << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Mdpi)
                                << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Hdpi)
                                << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Xhdpi)
                                << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Xxhdpi)
                                << QnResolutionUtil::densityMultiplier(QnResolutionUtil::Xxxhdpi);
        }

        int best = 0;
        for (int i = best + 1; i < standardMultipliers.size(); ++i) {
            if (qAbs(multiplier - standardMultipliers[i]) <= qAbs(multiplier - standardMultipliers[best]))
                best = i;
        }
        return static_cast<QnResolutionUtil::DensityClass>(best);
    }
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

QnResolutionUtil::QnResolutionUtil() {
    m_multiplier = ::densityMultiplier(&m_pixelRatio);
    m_densityClass = ::densityClass(m_multiplier * m_pixelRatio);
    m_classScale = m_multiplier * m_pixelRatio / densityMultiplier(m_densityClass);
}

QnResolutionUtil::QnResolutionUtil(QnResolutionUtil::DensityClass customDensityClass):
    m_densityClass(customDensityClass),
    m_multiplier(densityMultiplier(m_densityClass)),
    m_classScale(1.0),
    m_pixelRatio(1.0)
{
}

QnResolutionUtil::DensityClass QnResolutionUtil::densityClass() const {
    return m_densityClass;
}

QString QnResolutionUtil::densityName() const {
    return densityName(m_densityClass);
}

qreal QnResolutionUtil::densityMultiplier() const {
    return m_multiplier;
}

int QnResolutionUtil::dp(qreal dpix) const {
    return qRound(dpix * m_multiplier);
}

int QnResolutionUtil::sp(qreal dpix) const {
    return dp(dpix);
}

qreal QnResolutionUtil::classScale() const {
    return m_classScale;
}

qreal QnResolutionUtil::pixelRatio() const {
    return m_pixelRatio;
}
