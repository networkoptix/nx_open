#include "update_info.h"

QnVersion::QnVersion()
    : major(0), minor(0), build(0) {
}

QnVersion::QnVersion(const QString& strVersion) {
    major = minor = build = 0;

    QStringList versionList = strVersion.split(QLatin1Char('.'));

    if (versionList.size() > 0)
        major = versionList[0].toInt();

    if (versionList.size() > 1)
        minor = versionList[1].toInt();

    if (versionList.size() > 2)
        build = versionList[2].toInt();
}

bool QnVersion::isEmpty() const {
    return major == minor == build == 0;
}

bool QnVersion::operator<(const QnVersion& other) const {
    if (major < other.major) {
        return true;
    } else if (major == other.major) {
        if (minor < other.minor) {
            return true;
        } else if (minor == other.minor) {
            return build < other.build;
        }
    }

    return false;
}

QString QnVersion::toString() const {
    if (build)
        return QString(QLatin1String("%1.%2.%3")).arg(major).arg(minor).arg(build);
    else
        return QString(QLatin1String("%1.%2")).arg(major).arg(minor);
}
