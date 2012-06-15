#include "compatibility.h"

namespace
{
    QString stripVersion(const QString& version)
    {
        QStringList versionList = version.split(".");
        return QString("%1.%2").arg(versionList[0]).arg(versionList[1]);
    }
}

QnCompatibilityChecker::QnCompatibilityChecker(const QList<QnCompatibilityItem> compatiblityInfo)
{
    m_compatibilityMatrix = QnCompatibilityMatrixType::fromList(compatiblityInfo);
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

int QnCompatibilityChecker::size() const
{
    return m_compatibilityMatrix.size();
}
