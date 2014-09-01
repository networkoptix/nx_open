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

    QString toString(Format format = FullFormat) const;

    bool isNull() const;

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

    QN_FUSION_DECLARE_FUNCTIONS(QnSoftwareVersion, (ubjson)(xml)(json)(lexical)(binary)(datastream)(csv_field), friend)

private:
    std::array<int, 4> m_data;
};

Q_DECLARE_METATYPE(QnSoftwareVersion)

#endif // QN_SOFTWARE_VERSION_H
