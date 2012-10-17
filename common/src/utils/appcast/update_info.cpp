#include "update_info.h"

QnVersion::QnVersion(): 
    m_major(0), 
    m_minor(0), 
    m_bugfix(0) 
{}

QnVersion::QnVersion(int major, int minor, int bugfix):
    m_major(major),
    m_minor(minor),
    m_bugfix(bugfix)
{}

QnVersion::QnVersion(const QString &versionString) {
    m_major = m_minor = m_bugfix = 0;

    QStringList versionList = versionString.split(QLatin1Char('.'));

    if (versionList.size() > 0)
        m_major = versionList[0].toInt();

    if (versionList.size() > 1)
        m_minor = versionList[1].toInt();

    if (versionList.size() > 2)
        m_bugfix = versionList[2].toInt();
}

bool QnVersion::isNull() const {
    return m_major == 0 && m_minor == 0 && m_bugfix == 0;
}

bool QnVersion::operator<(const QnVersion& other) const {
    if (m_major < other.m_major) {
        return true;
    } else if (m_major == other.m_major) {
        if (m_minor < other.m_minor) {
            return true;
        } else if (m_minor == other.m_minor) {
            return m_bugfix < other.m_bugfix;
        }
    }

    return false;
}

bool QnVersion::operator==(const QnVersion &other) const {
    return m_major == other.m_major && m_minor == other.m_minor && m_bugfix == other.m_bugfix;
}

QString QnVersion::toString() const {
    if (m_bugfix)
        return QString(QLatin1String("%1.%2.%3")).arg(m_major).arg(m_minor).arg(m_bugfix);
    else
        return QString(QLatin1String("%1.%2")).arg(m_major).arg(m_minor);
}


bool QnUpdateInfoItem::isNull() const {
    return version.isNull();
}

bool QnUpdateInfoItem::operator==(const QnUpdateInfoItem &other) const {
    return version == other.version && title == other.title && description == other.description && pubDate == other.pubDate && url == other.url;
}
