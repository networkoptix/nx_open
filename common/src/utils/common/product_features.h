#ifndef QN_PRODUCT_FEATURES_H
#define QN_PRODUCT_FEATURES_H

#include <QtCore/QtGlobal>

#include <cstring> /* For strcmp. */
#include <cassert>

#include <version.h>

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
        result.customizationName = QLatin1String(QN_CUSTOMIZATION_NAME);
        result.freeLicenseIsTrial = QN_FREE_LICENSE_IS_TRIAL;
        result.freeLicenseCount = QN_FREE_LICENSE_COUNT;
        result.freeLicenseKey = QLatin1String(QN_FREE_LICENSE_KEY);
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
