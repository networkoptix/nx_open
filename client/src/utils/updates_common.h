#ifndef UPDATES_COMMON_H
#define UPDATES_COMMON_H

#include <utils/common/software_version.h>

struct QnUpdateFileInformation {
    QnSoftwareVersion version;
    QString fileName;
    qint64 fileSize;
    QString baseFileName;
    QUrl url;
    QString md5;

    QnUpdateFileInformation() {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const QString &fileName) :
        version(version), fileName(fileName)
    {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const QUrl &url) :
        version(version), url(url)
    {}
};
typedef QSharedPointer<QnUpdateFileInformation> QnUpdateFileInformationPtr;

#endif // UPDATES_COMMON_H
