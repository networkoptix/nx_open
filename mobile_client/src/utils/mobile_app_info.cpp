#include "mobile_app_info.h"

#include "utils/common/app_info.h"

QnMobileAppInfo::QnMobileAppInfo(QObject *parent) : QObject(parent) {

}

QString QnMobileAppInfo::productName() const {
    return QnAppInfo::productNameLong();
}

QString QnMobileAppInfo::organizationName() const {
    return QnAppInfo::organizationName();
}

QUrl QnMobileAppInfo::oldMobileClientUrl() const {
#if defined(Q_OS_ANDROID)
    return QnAppInfo::oldAndroidClientLink();
#elif defined(Q_OS_IOS)
    return QnAppInfo::oldIosClientLink();
#else
    return QUrl();
#endif
}
