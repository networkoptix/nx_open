#ifndef MERGE_SYSTEMS_TOOL_H
#define MERGE_SYSTEMS_TOOL_H

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>

class QnModuleInformation;

class QnMergeSystemsTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        InternalError = -1,
        NoError,
        VersionError,
        AuthentificationError,
        BackupError,
        JoinError,
        NotFoundError
    };

    explicit QnMergeSystemsTool(QObject *parent = 0);

    void pingSystem(const QUrl &url, const QString &password);
    void mergeSystem(const QUrl &url, const QString &password, bool ownSettings);

signals:
    void systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void mergeFinished(int errorCode);

private slots:
    void at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_mergeSystem_finished(int status, int handle);

private:
    QHash<int, QnMediaServerResourcePtr> m_serverByRequestHandle;
};

#endif // MERGE_SYSTEMS_TOOL_H
