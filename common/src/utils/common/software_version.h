#ifndef QN_SOFTWARE_VERSION_H
#define QN_SOFTWARE_VERSION_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>

#include <boost/operators.hpp>

class QnSoftwareVersion: public boost::equality_comparable1<QnSoftwareVersion, boost::less_than_comparable1<QnSoftwareVersion> > {
public:
    enum Format {
        StandardFormat,     /**< Standard version format, e.g. <tt>"MAJOR[.MINOR[.BUGFIX]]"</tt>. */
        OpenGlFormat        /**< OpenGL version format, with arbitrary text following the version string. */
    };

    QnSoftwareVersion(): m_major(0), m_minor(0), m_bugfix(0) {}

    QnSoftwareVersion(int major, int minor, int bugfix): m_major(major), m_minor(minor), m_bugfix(bugfix) {}

    explicit QnSoftwareVersion(const char *versionString, Format format = StandardFormat) {
        init(QLatin1String(versionString), format);
    }

    explicit QnSoftwareVersion(const QByteArray &versionString, Format format = StandardFormat) {
        init(QLatin1String(versionString.constData()), format);
    }

    explicit QnSoftwareVersion(const QString &versionString, Format format = StandardFormat) {
        init(versionString, format);
    }

    bool isNull() const {
        return m_major == 0 && m_minor == 0 && m_bugfix == 0;
    }

    bool operator<(const QnSoftwareVersion  &other) const {
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

    bool operator==(const QnSoftwareVersion &other) const {
        return m_major == other.m_major && m_minor == other.m_minor && m_bugfix == other.m_bugfix;
    }

    QString toString() const {
        if (m_bugfix)
            return QString(QLatin1String("%1.%2.%3")).arg(m_major).arg(m_minor).arg(m_bugfix);
        else
            return QString(QLatin1String("%1.%2")).arg(m_major).arg(m_minor);
    }

    int major() const {
        return m_major;
    }

    int minor() const {
        return m_minor;
    }

    int bugfix() const {
        return m_bugfix;
    }

    friend QDataStream &operator<<(QDataStream &stream, const QnSoftwareVersion &version) {
        return stream << version.major() << version.minor() << version.bugfix();
    }

    friend QDataStream &operator>>(QDataStream &stream, QnSoftwareVersion &version) {
        return stream >> version.m_major >> version.m_minor >> version.m_bugfix;
    }

private:
    void init(QString versionString, Format format) {
        m_major = m_minor = m_bugfix = 0;

        if(format == OpenGlFormat) {
            int spaceIndex = versionString.indexOf(QLatin1Char(' '));
            if(spaceIndex != -1)
                versionString = versionString.left(spaceIndex);
        }

        QStringList versionList = versionString.split(QLatin1Char('.'));

        if (versionList.size() > 0)
            m_major = versionList[0].toInt();

        if (versionList.size() > 1)
            m_minor = versionList[1].toInt();

        if (versionList.size() > 2)
            m_bugfix = versionList[2].toInt();
    }

private:
    int m_major;
    int m_minor;
    int m_bugfix;
};

Q_DECLARE_METATYPE(QnSoftwareVersion)

#endif // QN_SOFTWARE_VERSION_H
