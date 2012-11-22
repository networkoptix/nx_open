#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

namespace Ui {
    class ServerSettingsDialog;
}

namespace detail {


} // namespace detail


class QnServerSettingsDialog: public QnButtonBoxDialog {
    Q_OBJECT;

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

private:
    void updateFromResources();
    void submitToResources();

    int addTableRow(int id, const QString &url, int spaceLimitGb);

    void setTableStorages(const QnAbstractStorageResourceList &storages);
    QnAbstractStorageResourceList tableStorages() const;

    bool validateStorages(const QnAbstractStorageResourceList &storages);

    void updateSpaceLimitCell(int row, bool force = false);

private slots: 
    void at_storageAddButton_clicked();
    void at_storageRemoveButton_clicked();
    void at_storagesTable_cellChanged(int row, int column);

private:
    Q_DISABLE_COPY(QnServerSettingsDialog);

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    bool m_hasStorageChanges;
};

#endif // SERVER_SETTINGS_DIALOG_H
