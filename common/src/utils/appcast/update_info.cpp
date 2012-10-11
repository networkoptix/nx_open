#include "update_info.h"

QnVersion::QnVersion()
    : major_(0), minor_(0), build_(0) {
}

QnVersion::QnVersion(const QString& strVersion) {
    major_ = minor_ = build_ = 0;

    QStringList versionList = strVersion.split(QLatin1Char('.'));

    if (versionList.size() > 0)
        major_ = versionList[0].toInt();

    if (versionList.size() > 1)
        minor_ = versionList[1].toInt();

    if (versionList.size() > 2)
        build_ = versionList[2].toInt();
}

bool QnVersion::isEmpty() const {
    return major_ == minor_ == build_ == 0;
}

bool QnVersion::operator<(const QnVersion& other) const {
    if (major_ < other.major_) {
        return true;
    } else if (major_ == other.major_) {
        if (minor_ < other.minor_) {
            return true;
        } else if (minor_ == other.minor_) {
            return build_ < other.build_;
        }
    }

    return false;
}

QString QnVersion::toString() const {
    if (build_)
        return QString(QLatin1String("%1.%2.%3")).arg(major_).arg(minor_).arg(build_);
    else
        return QString(QLatin1String("%1.%2")).arg(major_).arg(minor_);
}
