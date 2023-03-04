// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/qobjectdefs.h>

// On some Linux systems "major" and "minor" are pre-defined macros in sys/types.h.
#undef major
#undef minor

namespace nx::utils {

struct NX_UTILS_API SoftwareVersion
{
    Q_GADGET
    Q_PROPERTY(int major MEMBER major CONSTANT)
    Q_PROPERTY(int minor MEMBER minor CONSTANT)
    Q_PROPERTY(int bugfix MEMBER bugfix CONSTANT)
    Q_PROPERTY(int build MEMBER build CONSTANT)

public:
    constexpr SoftwareVersion() {}

    constexpr SoftwareVersion(int major, int minor, int bugfix = 0, int build = 0):
        major(major), minor(minor), bugfix(bugfix), build(build)
    {
    }

    /**
     * Creates a software version object from a string. Note that this function
     * also supports OpenGL style version strings like "2.0.6914 WinXP SSE/SSE2/SSE3/3DNow!".
     *
     * @param versionString Version string. Can be empty which leads to zero version.
     */
    explicit SoftwareVersion(const QString& versionString);
    explicit SoftwareVersion(const char* versionString);
    explicit SoftwareVersion(const QByteArray& versionString);
    explicit SoftwareVersion(const std::string_view& versionString);

    enum class Format { minor, bugfix, full };
    Q_INVOKABLE QString toString(Format format = Format::full) const;

    Q_INVOKABLE bool isNull() const;

    bool deserialize(const QString& versionString);

    static SoftwareVersion fromStdString(const std::string& string);

    auto operator<=>(const SoftwareVersion&) const = default;

public:
    int major = 0;
    int minor = 0;
    int bugfix = 0;
    int build = 0;
};

NX_UTILS_API std::ostream& operator<<(std::ostream&, const SoftwareVersion&);

} // namespace nx::utils
