#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

class QLabel;

struct QnStorageSpaceReply;
struct QnStorageSpaceData;

namespace Ui {
    class ServerSettingsDialog;
}

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

    void addTableItem(const QnStorageSpaceData &item);
    void setTableItems(const QList<QnStorageSpaceData> &items);
    QList<QnStorageSpaceData> tableItems() const;

    //int addTableRow(int id, const QString &url, int spaceLimitGb);

    //void setTableStorages(const QnAbstractStorageResourceList &storages);
    //QnAbstractStorageResourceList tableStorages() const;

    //bool validateStorages(const QnAbstractStorageResourceList &storages);

    //void updateSpaceLimitCell(int row, bool force = false);

private slots: 
    void at_tableBottomLabel_linkActivated();
    void at_storagesTable_cellChanged(int row, int column);

    void at_replyReceived(int status, const QnStorageSpaceReply &reply, int handle);

private:
    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;
    QList<QString> m_storageProtocols;
    QLabel *m_tableBottomLabel;

    bool m_hasStorageChanges;
};

#endif // SERVER_SETTINGS_DIALOG_H
