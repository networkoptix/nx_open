#ifndef QN_UPDATE_INFO_H
#define QN_UPDATE_INFO_H

#include <QSharedPointer>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>

class QnVersion {
public:
    QnVersion();
    QnVersion(int major, int minor, int build);
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

    int build() const {
        return m_build;
    }

private:
    int m_major;
    int m_minor;
    int m_build;
};

// TODO: #Ivan add doxycomments for fields of this struct.
struct QnUpdateInfoItem {
    QnVersion version;
    QString title;
    QString description;
    QString pubDate;
    QUrl url;

    bool isNull() const;
    bool operator==(const QnUpdateInfoItem &other) const;
};


typedef QList<QnUpdateInfoItem> QnUpdateInfoItemList;

Q_DECLARE_METATYPE(QnUpdateInfoItemList)

#endif // QN_UPDATE_INFO_H
