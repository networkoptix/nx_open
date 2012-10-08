#ifndef _UPDATE_INFO_H
#define _UPDATE_INFO_H

#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>

struct QnVersion {
    QnVersion();
    QnVersion(const QString& strVersion);

    bool isEmpty() const;
    bool operator<(const QnVersion& other) const;
    QString toString() const;

    int major;
    int minor;
    int build;
};

struct QnUpdateInfoItem {
    QnVersion version;
    QString title;
    QString description;
    QString pubDate;
    QUrl url;
};

typedef QSharedPointer<QnUpdateInfoItem> QnUpdateInfoItemPtr;
typedef QList<QnUpdateInfoItemPtr> QnUpdateInfoItems;

Q_DECLARE_METATYPE(QnUpdateInfoItems)

#endif // _UPDATE_INFO_H