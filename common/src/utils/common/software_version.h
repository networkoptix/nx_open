#pragma once

#include <array>

#include <QtCore/QString>
#include <QtCore/QMetaType>

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

#include <nx/fusion/model_functions_fwd.h>

/* On some Linux systems "major" and "minor" are pre-defined macros in sys/types.h */
#undef major
#undef minor

class QnSoftwareVersion:
    public boost::equality_comparable1<
        QnSoftwareVersion, boost::less_than_comparable1<QnSoftwareVersion>>
{
    Q_GADGET
    Q_PROPERTY(int major READ major CONSTANT)
    Q_PROPERTY(int minor READ minor CONSTANT)
    Q_PROPERTY(int bugfix READ bugfix CONSTANT)
    Q_PROPERTY(int build READ build CONSTANT)

public:
    enum Format
    {
        FullFormat = 4,
        BugfixFormat = 3,
        MinorFormat = 2
    };
    Q_ENUM(Format)

    QnSoftwareVersion();

    QnSoftwareVersion(int major, int minor, int bugfix = 0, int build = 0);

    /**
     * Creates a software version object from a string. Note that this function
     * also supports OpenGL style version strings like "2.0.6914 WinXP SSE/SSE2/SSE3/3DNow!".
     *
     * \param versionString             Version string.
     */
    explicit QnSoftwareVersion(const QString &versionString);
    explicit QnSoftwareVersion(const char *versionString);
    explicit QnSoftwareVersion(const QByteArray &versionString);

    Q_INVOKABLE QString toString(Format format = FullFormat) const;

    Q_INVOKABLE bool isNull() const;

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

    bool isCompatible(const QnSoftwareVersion &r) const
    {
        return m_data[0] == r.m_data[0] && m_data[1] == r.m_data[1];
    }

    friend bool isCompatible(const QnSoftwareVersion &l, const QnSoftwareVersion &r)
    {
        return l.isCompatible(r);
    }

    Q_INVOKABLE bool isLessThan(const QnSoftwareVersion& other) const;

    friend bool operator<(const QnSoftwareVersion &l, const QnSoftwareVersion &r);
    friend bool operator==(const QnSoftwareVersion &l, const QnSoftwareVersion &r);

    QN_FUSION_DECLARE_FUNCTIONS(QnSoftwareVersion, (ubjson)(xml)(json)(lexical)(datastream)(csv_field), friend)

private:
    std::array<int, 4> m_data;
};

Q_DECLARE_METATYPE(QnSoftwareVersion)
