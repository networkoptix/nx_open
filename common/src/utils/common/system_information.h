#ifndef SYSTEM_INFORMATION_H
#define SYSTEM_INFORMATION_H

#include <QtCore/QString>

#include <utils/common/model_functions_fwd.h>

class QnSystemInformation {
public:
    QnSystemInformation(const QString &platform, const QString &arch);
    QnSystemInformation(const QString &infoString);
    QnSystemInformation() {}

    QString toString() const;
    bool isValid() const;

    QString arch;
    QString platform;
};
#define QnSystemInformation_Fields (arch)(platform)

QN_FUSION_DECLARE_FUNCTIONS(QnSystemInformation, (json)(binary)(datastream)(eq)(hash)(metatype))

#endif // SYSTEM_INFORMATION_H
