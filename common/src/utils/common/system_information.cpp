#include "system_information.h"

#include <QtCore/QRegExp>

#include <utils/common/model_functions.h>

#include "version.h"

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSystemInformation, (json)(binary)(datastream)(eq)(hash), QnSystemInformation_Fields, (optional, true))

QnSystemInformation::QnSystemInformation(const QString &platform, const QString &arch, const QString &modification) :
    arch(arch),
    platform(platform),
    modification(modification)
{}

QnSystemInformation::QnSystemInformation(const QString &infoString) {
    QRegExp infoRegExp(lit("(\\S+)\\s+(\\S+)\\s*(\\S*)"));
    if (infoRegExp.exactMatch(infoString)) {
        platform = infoRegExp.cap(1);
        arch = infoRegExp.cap(2);
        modification = infoRegExp.cap(3);
    }
}

QString QnSystemInformation::toString() const {
    QString result = platform + lit(" ") + arch;
    if (!modification.isEmpty())
        result.append(lit(" ") + modification);
    return result;
}

bool QnSystemInformation::isValid() const {
    return !platform.isEmpty() && !arch.isEmpty();
}

QnSystemInformation QnSystemInformation::currentSystemInformation() {
    QString arch(lit(QN_APPLICATION_ARCH));
    QString mod;
    if (arch == lit("arm"))
        mod = lit(QN_ARM_BOX);
    return QnSystemInformation(lit(QN_APPLICATION_PLATFORM), arch, mod);
}
