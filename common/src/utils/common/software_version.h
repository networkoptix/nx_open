#ifndef QN_SOFTWARE_VERSION_H
#define QN_SOFTWARE_VERSION_H

#include <algorithm>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>

#include <boost/array.hpp>
#include <boost/operators.hpp>

class QnSoftwareVersion: public boost::equality_comparable1<QnSoftwareVersion, boost::less_than_comparable1<QnSoftwareVersion> > {
public:
    enum Format {
        FullFormat = 4,
        BugfixFormat = 3,
        MinorFormat = 2
    };

    QnSoftwareVersion() { 
        std::fill(m_data.begin(), m_data.end(), 0); 
    }

    QnSoftwareVersion(int major, int minor, int bugfix, int build) {
        m_data[0] = major;
        m_data[1] = minor;
        m_data[2] = bugfix;
        m_data[3] = build;
    }

    explicit QnSoftwareVersion(const char *versionString) {
        init(QLatin1String(versionString));
    }

    explicit QnSoftwareVersion(const QByteArray &versionString) {
        init(QLatin1String(versionString.constData()));
    }

    /**
     * Creates a software version object from a string. Note that this function
     * also supports OpenGL style version strings like "2.0.6914 WinXP SSE/SSE2/SSE3/3DNow!".
     * 
     * \param versionString             Version string.
     */
    explicit QnSoftwareVersion(const QString &versionString) {
        init(versionString);
    }

    bool isNull() const {
        return m_data[0] == 0 && m_data[1] == 0 && m_data[2] == 0 && m_data[3] == 0;
    }

    bool operator<(const QnSoftwareVersion  &other) const {
        return std::lexicographical_compare(m_data.begin(), m_data.end(), other.m_data.begin(), other.m_data.end());
    }

    bool operator==(const QnSoftwareVersion &other) const {
        return std::equal(m_data.begin(), m_data.end(), other.m_data.begin());
    }
    
    QString toString(Format format = FullFormat) const {
        QString result = QString::number(m_data[0]);
        for(int i = 1; i < format; i++)
            result += QLatin1Char('.') + QString::number(m_data[i]);
        return result;
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

    friend QDataStream &operator<<(QDataStream &stream, const QnSoftwareVersion &version) {
        for(int i = 0; i < 4; i++)
            stream << version.m_data[i];
        return stream;
    }

    friend QDataStream &operator>>(QDataStream &stream, QnSoftwareVersion &version) {
        for(int i = 0; i < 4; i++)
            stream >> version.m_data[i];
        return stream;
    }

    friend bool isCompatible(const QnSoftwareVersion &l, const QnSoftwareVersion &r) {
        return l.m_data[0] == r.m_data[0] && l.m_data[1] == r.m_data[1];
    }

private:
    void init(const QString &versionString) {
        std::fill(m_data.begin(), m_data.end(), 0);

        QString s = versionString.trimmed();
        int index = s.indexOf(L' ');
        if(index != -1)
            s = s.mid(0, index);

        QStringList versionList = s.split(QLatin1Char('.'));
        for(int i = 0, count = qMin(4, versionList.size()); i < count; i++)
            m_data[i] = versionList[i].toInt();
    }

private:
    boost::array<int, 4> m_data;
};

Q_DECLARE_METATYPE(QnSoftwareVersion)

#endif // QN_SOFTWARE_VERSION_H
