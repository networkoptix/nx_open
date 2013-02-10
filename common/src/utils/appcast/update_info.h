#ifndef QN_UPDATE_INFO_H
#define QN_UPDATE_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QUrl>

#include <boost/operators.hpp>

#include <utils/common/version.h>


struct QnUpdateInfoItem: public boost::equality_comparable1<QnUpdateInfoItem> {
    /** Product version of the update. This is the version to be displayed to the user. */
    QnVersion productVersion;

    /** Engine version of the update. */
    QnVersion engineVersion;

    /** Update information title. */
    QString title;

    /** Detailed update information. */
    QString description;

    /** Date when this update was released. */
    QString pubDate;

    /** Download url. */
    QUrl url;

    bool isNull() const {
        return engineVersion.isNull();
    }

    bool operator==(const QnUpdateInfoItem &other) const {
        return 
            productVersion == other.productVersion && 
            engineVersion == other.engineVersion &&
            title == other.title && 
            description == other.description && 
            pubDate == other.pubDate && 
            url == other.url;
    }
};


typedef QList<QnUpdateInfoItem> QnUpdateInfoItemList;

Q_DECLARE_METATYPE(QnUpdateInfoItemList)

#endif // QN_UPDATE_INFO_H
