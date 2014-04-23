#ifndef _COMMON_COMPATIBILITY_H
#define _COMMON_COMPATIBILITY_H

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QList>

#include <utils/common/software_version.h>


// Presense of an entry in global table means
// that component of ver1 is compatible (or has compatibility mode)
// with EVERY component of ver2

QString stripVersion(const QString& version);

#include "compatibility_i.h"

inline uint qHash(const QnCompatibilityItem &item)
{
    return qHash(item.ver1) + 7 * qHash(item.comp1) + 11 * qHash(item.ver2);
}

class QnCompatibilityChecker
{
public:
    QnCompatibilityChecker(const QList<QnCompatibilityItem> compatiblityInfo);

    //TODO: #Ivan write comments PLEASE! what comp1 and comp2 mean and should they be translated?
    bool isCompatible(const QString& comp1, const QString& ver1, const QString& comp2, const QString& ver2) const;
    bool isCompatible(const QString& comp1, const QnSoftwareVersion& ver1, const QString& comp2, const QnSoftwareVersion& ver2) const;
    int size() const;

private:
    typedef QSet<QnCompatibilityItem> QnCompatibilityMatrixType;
    QnCompatibilityMatrixType m_compatibilityMatrix;
};

// This functions is defined in generated compatibility_info.cpp file.
QList<QnCompatibilityItem> localCompatibilityItems();

#endif // _COMMON_COMPATIBILITY_H
