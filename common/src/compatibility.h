#ifndef _COMMON_COMPATIBILITY_H
#define _COMMON_COMPATIBILITY_H

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QList>

#include <utils/common/software_version.h>
#include <utils/common/model_functions_fwd.h>
#include <api/model/compatibility_item.h>

// TODO: #Elric move and rename file.

// Presence of an entry in global table means
// that component of ver1 is compatible (or has compatibility mode)
// with EVERY component of ver2

class QnCompatibilityChecker
{
public:
    QnCompatibilityChecker(const QList<QnCompatibilityItem> compatiblityInfo);

    //TODO: #Ivan write comments PLEASE! what comp1 and comp2 mean and should they be translated?
    bool isCompatible(const QString& comp1, const QString& ver1, const QString& comp2, const QString& ver2) const;
    bool isCompatible(const QString& comp1, const QnSoftwareVersion& ver1, const QString& comp2, const QnSoftwareVersion& ver2) const;
    int size() const;

private:
    QSet<QnCompatibilityItem> m_compatibilityMatrix;
};


QString stripVersion(const QString& version);

// This functions is defined in generated compatibility_info.cpp file.
QList<QnCompatibilityItem> localCompatibilityItems();

#endif // _COMMON_COMPATIBILITY_H
