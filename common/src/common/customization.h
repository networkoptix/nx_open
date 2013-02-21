#ifndef QN_COMMON_CUSTOMIZATION_H
#define QN_COMMON_CUSTOMIZATION_H

#include <QtCore/QtGlobal>

#include <cstring> /* For strcmp. */
#include <cassert>

#include <version.h>

struct QnProductFeatures {
    int freeLicenseCount;
    QString freeLicenseKey;
};


namespace Qn {
    enum Customization {
        HdWitnessCustomization,
        DwSpectrumCustomization,
        NVisionCustomization
    };

    inline Qn::Customization calculateCustomization(const char *customizationName) {
        if(std::strcmp(customizationName, "Vms") == 0) {
            return HdWitnessCustomization;
        } else if(std::strcmp(customizationName, "digitalwatchdog") == 0) {
            return DwSpectrumCustomization;
        } else if(std::strcmp(customizationName, "nnodal") == 0) {
            return NVisionCustomization;
        } else {
            assert(false);
            return HdWitnessCustomization;
        }
    }

    inline QnProductFeatures calculateProductFeatures() {
        QnProductFeatures result;
        result.freeLicenseCount = QN_FREE_LICENSE_COUNT;
        result.freeLicenseKey = QLatin1String(QN_FREE_LICENSE_KEY);
        return result;
    }

} // namespace Qn

/**
 * \returns                             Customization with which this software
 *                                      version was built.
 */
inline Qn::Customization qnCustomization() {
    /* Note that Q_GLOBAL_STATIC and other synchronization is not needed here. */
    static Qn::Customization result = Qn::calculateCustomization(QN_CUSTOMIZATION_NAME);
    return result;
}

/**
 * \returns                             Product features that are available for
 *                                      customization with which this software
 *                                      version was built.
 */
inline QnProductFeatures qnProductFeatures() {
    /* Note that Q_GLOBAL_STATIC and other synchronization is not needed here. */
    static QnProductFeatures result = Qn::calculateProductFeatures();
    return result;
}


#endif // QN_COMMON_CUSTOMIZATION_H
