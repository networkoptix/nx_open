#ifndef QN_UPDATE_INFO_H
#define QN_UPDATE_INFO_H

#include <QSharedPointer>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>

// TODO: use boost to implement operators.
class QnVersion {
public:
    QnVersion();
    QnVersion(int major, int minor, int bugfix);
    QnVersion(const QString &versionString);

    bool isNull() const;
    bool operator<(const QnVersion  &other) const;
    bool operator==(const QnVersion &other) const;

    QString toString() const;

    int major() const {
        return m_major;
    }

    int minor() const {
        return m_minor;
    }

    int bugfix() const {
        return m_bugfix;
    }

private:
    int m_major;
    int m_minor;
    int m_bugfix;
};


struct QnUpdateInfoItem {
    /** Update version. */
    QnVersion version;

    /** Update information title. */
    QString title;

    /** Detailed update information. */
    QString description;

    /** Date when this update was released. */
    QString pubDate;

    /** Download url. */
    QUrl url;

    bool isNull() const;
    bool operator==(const QnUpdateInfoItem &other) const;
};


typedef QList<QnUpdateInfoItem> QnUpdateInfoItemList;

Q_DECLARE_METATYPE(QnUpdateInfoItemList)

#endif // QN_UPDATE_INFO_H
