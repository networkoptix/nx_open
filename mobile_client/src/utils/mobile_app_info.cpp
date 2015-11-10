#include "mobile_app_info.h"

#include "utils/common/app_info.h"

QnMobileAppInfo::QnMobileAppInfo(QObject *parent) : QObject(parent) {

}

QString QnMobileAppInfo::productName() const {
    return QnAppInfo::productNameLong();
}

