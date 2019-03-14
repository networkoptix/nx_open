#ifndef QN_STORAGE_URL_DIALOG_H
#define QN_STORAGE_URL_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include "core/resource/resource_fwd.h"

#include "api/model/storage_status_reply.h"

#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/models/storage_model_info.h>

struct QnStorageStatusReply;
namespace nx::vms::client::desktop { class BusyIndicatorButton; }
namespace Ui { class StorageUrlDialog; }

class QnStorageUrlDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    QnStorageUrlDialog(const QnMediaServerResourcePtr& server, QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~QnStorageUrlDialog();

    QSet<QString> protocols() const;
    void setProtocols(const QSet<QString> &protocols);

    /**
     * Current server storages can be modified but not saved,
     * so we are passing current state right here to avoid
     * invalid storageAlreadyUsed() results.
     */
    QnStorageModelInfoList currentServerStorages() const;
    void setCurrentServerStorages(const QnStorageModelInfoList &storages);

    QnStorageModelInfo storage() const;

protected:
    virtual void accept() override;

private:
    struct ProtocolDescription
    {
        QString protocol;
        QString name;
        QString urlTemplate;
        QString urlPattern;
    };

    static ProtocolDescription protocolDescription(const QString& protocol);

    QString makeUrl(const QString& path, const QString& login, const QString& password);
    bool storageAlreadyUsed(const QString& path) const;

private slots:
    void updateComboBox();

    void at_protocolComboBox_currentIndexChanged();
    QString normalizePath(QString path);

private:
    QScopedPointer<Ui::StorageUrlDialog> ui;
    QnMediaServerResourcePtr m_server;
    QSet<QString> m_protocols;
    QList<ProtocolDescription> m_descriptions;
    QHash<QString, QString> m_urlByProtocol;
    QString m_lastProtocol;
    QnStorageModelInfo m_storage;
    QnStorageModelInfoList m_currentServerStorages;
    nx::vms::client::desktop::BusyIndicatorButton* const m_okButton = nullptr;
};

#endif // QN_STORAGE_URL_DIALOG_H
