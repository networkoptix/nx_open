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

    inline Qn::QnProductFeatures calculateProductFeatures(Qn::Customization customization) {
        Q_UNUSED(customization);

        QnProductFeatures result;
        if(QN_HAS_FREE_LICENSES)
            result |= FreeLicenseFeature;
        return result;
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
