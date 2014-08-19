#ifndef QN_STORAGE_URL_DIALOG_H
#define QN_STORAGE_URL_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include "core/resource/abstract_storage_resource.h"
#include "core/resource/resource_fwd.h"

#include "api/model/storage_status_reply.h"

#include <ui/dialogs/workbench_state_dependent_dialog.h>

struct QnStorageStatusReply;

namespace Ui {
    class StorageUrlDialog;
}

class QnStorageUrlDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnStorageUrlDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnStorageUrlDialog();

    QList<QString> protocols() const;
    void setProtocols(const QList<QString> &protocols);

    QnStorageSpaceData storage() const;

protected:
    virtual void accept() override;
private:
    QString makeUrl(const QString& path, const QString& login, const QString& password);
private slots:
    void updateComboBox();

    void at_protocolComboBox_currentIndexChanged();
    QString normalizePath(QString path);
private:
    QScopedPointer<Ui::StorageUrlDialog> ui;
    QnMediaServerResourcePtr m_server;
    QList<QString> m_protocols;
    QList<QnAbstractStorageResource::ProtocolDescription> m_descriptions;
    QHash<QString, QString> m_urlByProtocol;
    QString m_lastProtocol;
    QnStorageSpaceData m_storage;
};

#endif // QN_STORAGE_URL_DIALOG_H
