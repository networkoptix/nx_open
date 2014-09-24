#include "compatibility.h"

#include <QtCore/QStringList>

inline uint qHash(const QnCompatibilityItem &item)
{
    return qHash(item.ver1) + 7 * qHash(item.comp1) + 11 * qHash(item.ver2);
}

QString stripVersion(const QString& version)
{
    QStringList versionList = version.split(QLatin1Char('.'));
    return QString(QLatin1String("%1.%2")).arg(versionList[0]).arg(versionList[1]);
}

QnCompatibilityChecker::QnCompatibilityChecker(const QList<QnCompatibilityItem> compatiblityInfo)
{
    m_compatibilityMatrix = compatiblityInfo.toSet();
}

bool QnCompatibilityChecker::isCompatible(const QString& comp1, const QString& ver1, const QString& comp2, const QString& ver2) const
{
    QString ver1s = stripVersion(ver1);
    QString ver2s = stripVersion(ver2);

    if (ver1s == ver2s)
        return true;

    return m_compatibilityMatrix.contains(QnCompatibilityItem(ver1s, comp1, ver2s)) ||
            m_compatibilityMatrix.contains(QnCompatibilityItem(ver2s, comp2, ver1s));
}

bool QnCompatibilityChecker::isCompatible(const QString& comp1, const QnSoftwareVersion& ver1, const QString& comp2, const QnSoftwareVersion& ver2) const {
    return isCompatible(comp1, ver1.toString(), comp2, ver2.toString());
}

int QnCompatibilityChecker::size() const
{
    return m_compatibilityMatrix.size();
}
