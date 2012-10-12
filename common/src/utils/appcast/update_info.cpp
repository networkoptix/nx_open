#include "update_info.h"

QnVersion::QnVersion(): 
    m_major(0), 
    m_minor(0), 
    m_build(0) 
{}

QnVersion::QnVersion(int major, int minor, int build):
    m_major(major),
    m_minor(minor),
    m_build(build)
{}

QnVersion::QnVersion(const QString &versionString) {
    m_major = m_minor = m_build = 0;

    QStringList versionList = versionString.split(QLatin1Char('.'));

    if (versionList.size() > 0)
        m_major = versionList[0].toInt();

    if (versionList.size() > 1)
        m_minor = versionList[1].toInt();

    if (versionList.size() > 2)
        m_build = versionList[2].toInt();
}

bool QnVersion::isNull() const {
    return m_major == 0 && m_minor == 0 && m_build == 0;
}

bool QnVersion::operator<(const QnVersion& other) const {
    if (m_major < other.m_major) {
        return true;
    } else if (m_major == other.m_major) {
        if (m_minor < other.m_minor) {
            return true;
        } else if (m_minor == other.m_minor) {
            return m_build < other.m_build;
        }
    }

    return false;
}

bool QnVersion::operator==(const QnVersion &other) const {
    return m_major == other.m_major && m_minor == other.m_minor && m_build == other.m_build;
}

QString QnVersion::toString() const {
    if (m_build)
        return QString(QLatin1String("%1.%2.%3")).arg(m_major).arg(m_minor).arg(m_build);
    else
        return QString(QLatin1String("%1.%2")).arg(m_major).arg(m_minor);
}


bool QnUpdateInfoItem::isNull() const {
    return version.isNull();
}

bool QnUpdateInfoItem::operator==(const QnUpdateInfoItem &other) const {
    return version == other.version && title == other.title && description == other.description && pubDate == other.pubDate && url == other.url;
}
