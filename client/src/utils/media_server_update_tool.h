#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>

// TODO: add tracking to the newly added servers

class QnMediaServerUpdateTool : public QObject
{
    Q_OBJECT
public:
    explicit QnMediaServerUpdateTool(QObject *parent = 0);

    QString updateFile() const;
    void setUpdateFile(const QString &fileName);

    void updateServers();

private slots:
    void updateUploaded(int status, const QString &reply, int handle);

private:
    QString m_updateFile;
    QHash<QString, QnMediaServerResourcePtr> m_pendingUploadServers;
    QHash<QString, QnMediaServerResourcePtr> m_pendingInstallServers;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
