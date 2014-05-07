#ifndef QN_SOFTWARE_VERSION_H
#define QN_SOFTWARE_VERSION_H

#include <array>

#ifndef QN_NO_QT
#include <QtCore/QString>
#include <QtCore/QMetaType>
#endif

#include <boost/operators.hpp>

#include <utils/common/model_functions_fwd.h>


class QnSoftwareVersion: public boost::equality_comparable1<QnSoftwareVersion, boost::less_than_comparable1<QnSoftwareVersion> > {
public:
    enum Format {
        FullFormat = 4,
        BugfixFormat = 3,
        MinorFormat = 2
    };

    QnSoftwareVersion() { 
        m_data[0] = m_data[1] = m_data[2] = m_data[3] = 0;
    }

    QnSoftwareVersion(int major, int minor, int bugfix, int build) {
        m_data[0] = major;
        m_data[1] = minor;
        m_data[2] = bugfix;
        m_data[3] = build;
    }

    /**
     * Creates a software version object from a string. Note that this function
     * also supports OpenGL style version strings like "2.0.6914 WinXP SSE/SSE2/SSE3/3DNow!".
     * 
     * \param versionString             Version string.
     */
    explicit QnSoftwareVersion(const QString &versionString) {
        deserialize(versionString, this);
    }

    explicit QnSoftwareVersion(const char *versionString) {
        deserialize(QString(QLatin1String(versionString)), this);
    }

    explicit QnSoftwareVersion(const QByteArray &versionString) {
        deserialize(QString(QLatin1String(versionString.constData())), this);
    }

    QString toString(Format format = FullFormat) const;

    bool isNull() const {
        return m_data[0] == 0 && m_data[1] == 0 && m_data[2] == 0 && m_data[3] == 0;
    }

    int major() const {
        return m_data[0];
    }

    int minor() const {
        return m_data[1];
    }

    int bugfix() const {
        return m_data[2];
    }

    int build() const {
        return m_data[3];
    }

    friend bool isCompatible(const QnSoftwareVersion &l, const QnSoftwareVersion &r) {
        return l.m_data[0] == r.m_data[0] && l.m_data[1] == r.m_data[1];
    }

    friend bool operator<(const QnSoftwareVersion &l, const QnSoftwareVersion &r);
    friend bool operator==(const QnSoftwareVersion &l, const QnSoftwareVersion &r);

    QN_FUSION_DECLARE_FUNCTIONS(QnSoftwareVersion, (json)(lexical)(binary)(datastream)(csv_field), friend)

private:
    std::array<int, 4> m_data;
};

Q_DECLARE_METATYPE(QnSoftwareVersion)

#endif // QN_SOFTWARE_VERSION_H
