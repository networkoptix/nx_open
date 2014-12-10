#ifndef MERGE_SYSTEMS_TOOL_H
#define MERGE_SYSTEMS_TOOL_H

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

struct QnModuleInformation;

class QnMergeSystemsTool : public QObject, public QnWorkbenchContextAware {
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
    int mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &user, const QString &password, bool ownSettings, bool oneServer = false);

signals:
    void systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int requestHandle);

private slots:
    void at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);

private:
    QHash<int, QnMediaServerResourcePtr> m_serverByRequestHandle;
    QString m_password;
};

#endif // MERGE_SYSTEMS_TOOL_H
