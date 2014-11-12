#ifndef MERGE_SYSTEMS_TOOL_H
#define MERGE_SYSTEMS_TOOL_H

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>

struct QnModuleInformation;

class QnMergeSystemsTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        InternalError = -1,
        NoError,
        VersionError,
        AuthentificationError,
        BackupError,
        MergeError,
        NotFoundError
    };

    explicit QnMergeSystemsTool(QObject *parent = 0);

    void pingSystem(const QUrl &url, const QString &user, const QString &password);
    void mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &user, const QString &password, bool ownSettings);

signals:
    void systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void mergeFinished(int errorCode, const QnModuleInformation &moduleInformation);

private slots:
    void at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);

private:
    QHash<int, QnMediaServerResourcePtr> m_serverByRequestHandle;
};

#endif // MERGE_SYSTEMS_TOOL_H
