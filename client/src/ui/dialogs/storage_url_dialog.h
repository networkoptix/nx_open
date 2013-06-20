#ifndef QN_STORAGE_URL_DIALOG_H
#define QN_STORAGE_URL_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include "core/resource/abstract_storage_resource.h"
#include "core/resource/resource_fwd.h"

#include "api/model/storage_status_reply.h"

struct QnStorageStatusReply;

namespace Ui {
    class StorageUrlDialog;
}

class QnStorageUrlDialog: public QDialog {
    Q_OBJECT

    typedef QDialog base_type;

public:
    QnStorageUrlDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnStorageUrlDialog();

    QList<QString> protocols() const;
    void setProtocols(const QList<QString> &protocols);

    QnStorageSpaceData storage() const;

protected:
    virtual void accept() override;

private slots:
    void updateComboBox();

    void at_protocolComboBox_currentIndexChanged();

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
