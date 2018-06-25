#pragma once

#include <array>

#include <QtCore/QString>

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

/* On some Linux systems "major" and "minor" are pre-defined macros in sys/types.h */
#undef major
#undef minor

namespace nx {
namespace utils {

class NX_UTILS_API SoftwareVersion:
    public boost::equality_comparable1<SoftwareVersion,
        boost::less_than_comparable1<SoftwareVersion>>
{
public:
    enum Format
    {
        FullFormat = 4,
        BugfixFormat = 3,
        MinorFormat = 2
    };

    SoftwareVersion();

    SoftwareVersion(int major, int minor, int bugfix = 0, int build = 0);

    /**
     * Creates a software version object from a string. Note that this function
     * also supports OpenGL style version strings like "2.0.6914 WinXP SSE/SSE2/SSE3/3DNow!".
     *
     * @param versionString Version string. Can be empty which leads to zero version.
     */
    explicit SoftwareVersion(const QString& versionString);
    explicit SoftwareVersion(const char* versionString);
    explicit SoftwareVersion(const QByteArray& versionString);

    QString toString(Format format = FullFormat) const;

    bool isNull() const;

    int major() const
    {
        return m_data[0];
    }

    int minor() const
    {
        return m_data[1];
    }

    int bugfix() const
    {
        return m_data[2];
    }

    int build() const
    {
        return m_data[3];
    }

    friend bool NX_UTILS_API operator<(const SoftwareVersion& l, const SoftwareVersion& r);
    friend bool NX_UTILS_API operator==(const SoftwareVersion& l, const SoftwareVersion& r);

protected:
    std::array<int, 4> m_data;
};

} // namespace utils
} // namespace nx
