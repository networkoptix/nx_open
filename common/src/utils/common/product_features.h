#ifndef QN_PRODUCT_FEATURES_H
#define QN_PRODUCT_FEATURES_H

#include <QtCore/QtGlobal>

#include <cstring> /* For strcmp. */
#include <cassert>

#include <utils/common/app_info.h>

struct QnProductFeatures {
    QString customizationName;
    bool freeLicenseIsTrial;
    int freeLicenseCount;
    QString freeLicenseKey;
    int actionAggregationPeriod;
};


namespace Qn {

    inline QnProductFeatures calculateProductFeatures() {
        QnProductFeatures result;
        result.customizationName = QnAppInfo::customizationName();
        result.freeLicenseIsTrial = QnAppInfo::freeLicenseIsTrial();
        result.freeLicenseCount = QnAppInfo::freeLicenseCount();
        result.freeLicenseKey = QnAppInfo::freeLicenseKey();
        return result;
    }

} // namespace Qn

/**
 * \returns                             Product features that are available for
 *                                      customization with which this software
 *                                      version was built.
 */
inline QnProductFeatures qnProductFeatures() {
    static QnProductFeatures result = Qn::calculateProductFeatures(); // TODO: #Elric synch is actually needed!
    return result;
}


#endif // QN_PRODUCT_FEATURES_H
