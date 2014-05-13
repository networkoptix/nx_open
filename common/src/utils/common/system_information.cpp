#include "system_information.h"

#include <QtCore/QRegExp>

#include <utils/common/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSystemInformation, (json)(binary)(datastream)(eq)(hash), QnSystemInformation_Fields)

QnSystemInformation::QnSystemInformation(const QString &platform, const QString &arch) :
    arch(arch),
    platform(platform)
{}

QnSystemInformation::QnSystemInformation(const QString &infoString) {
    QRegExp infoRegExp(lit("(.+) (.+)"));
    if (infoRegExp.exactMatch(infoString)) {
        platform = infoRegExp.cap(1);
        arch = infoRegExp.cap(2);
    }
}

QString QnSystemInformation::toString() const {
    return platform + lit(" ") + arch;
}

bool QnSystemInformation::isValid() const {
    return !platform.isEmpty() && !arch.isEmpty();
}
