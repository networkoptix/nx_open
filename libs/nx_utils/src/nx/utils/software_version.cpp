// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <ostream>

#include "software_version.h"

namespace nx::utils {

SoftwareVersion::SoftwareVersion(const QString& versionString)
{
    deserialize(versionString);
}

SoftwareVersion::SoftwareVersion(const char* versionString):
    SoftwareVersion(QString::fromLatin1(versionString))
{
}

SoftwareVersion::SoftwareVersion(const QByteArray& versionString):
    SoftwareVersion(QString::fromUtf8(versionString))
{
}

SoftwareVersion::SoftwareVersion(const std::string_view& versionString):
    SoftwareVersion(QString::fromUtf8(versionString.data(), versionString.size()))
{
}

QString SoftwareVersion::toString(SoftwareVersion::Format format) const
{
    QString result = QString::number(major) + "." + QString::number(minor);
    if (format > Format::minor)
        result += "." + QString::number(bugfix);
    if (format > Format::bugfix)
        result += "." + QString::number(build);
    return result;
}

SoftwareVersion SoftwareVersion::fromStdString(const std::string& string)
{
    return SoftwareVersion(QString::fromStdString(string));
}

bool SoftwareVersion::isNull() const
{
    return major == 0 && minor == 0 && bugfix == 0 && build == 0;
}

bool SoftwareVersion::deserialize(const QString& versionString)
{
    // Strip everything after the first space.
    QString s = versionString.trimmed();
    int index = s.indexOf(' ');
    if (index != -1)
        s.truncate(index);

    // Set segment values.

    bool ok = true;
    auto setSegment =
        [index = 0, s, &ok](int& segment) mutable
        {
            if (index == -1)
                return;

            const int dotIndex = s.indexOf('.', index);
            bool segmentOk = false;
            segment = s.mid(index, dotIndex - index).toInt(&segmentOk);
            ok &= segmentOk;
            index = dotIndex >= 0 ? dotIndex + 1 : -1;
        };

    setSegment(major);
    setSegment(minor);
    setSegment(bugfix);
    setSegment(build);

    return ok;
}

std::ostream& operator<<(std::ostream& stream, const SoftwareVersion& version)
{
    return stream << version.toString().toStdString();
}

} // namespace nx::utils
