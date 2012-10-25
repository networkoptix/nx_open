#ifndef QN_COMMON_CUSTOMIZATION_H
#define QN_COMMON_CUSTOMIZATION_H

#include <QtCore/QtGlobal>

#include <cstring> /* For strcmp. */
#include <cassert>

#include <version.h>

namespace Qn {
    enum Customization {
        HdWitnessCustomization,
        DwSpectrumCustomization,
        NVisionCustomization
    };

    enum QnProductFeature {
        FreeLicenseFeature = 0x1
    };
    Q_DECLARE_FLAGS(QnProductFeatures, QnProductFeature);


    Qn::Customization calculateCustomization(const char *customizationName) {
        if(std::strcmp(customizationName, "Vms")) {
            return HdWitnessCustomization;
        } else if(std::strcmp(customizationName, "digitalwatchdog")) {
            return DwSpectrumCustomization;
        } else if(std::strcmp(customizationName, "nnodal")) {
            return NVisionCustomization;
        } else {
            assert(false);
            return HdWitnessCustomization;
        }
    }

    Qn::QnProductFeatures calculateProductFeatures(Qn::Customization customization) {
        switch(customization) {
        case HdWitnessCustomization:
            return FreeLicenseFeature;
        case DwSpectrumCustomization:
            return QnProductFeatures();
        case NVisionCustomization:
            return FreeLicenseFeature;
        default:
            assert(false);
            return 0;
        }
    }

} // namespace Qn

/**
 * \returns                             Customization with which this software
 *                                      version was built with.
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
inline Qn::QnProductFeatures qnProductFeatures() {
    /* Note that Q_GLOBAL_STATIC and other synchronization is not needed here. */
    static Qn::QnProductFeatures result = Qn::calculateProductFeatures(qnCustomization());
    return result;
}


#endif // QN_COMMON_CUSTOMIZATION_H
