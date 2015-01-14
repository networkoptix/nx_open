#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H


#include <QtWidgets/QDialog>
#include <QTimer>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "button_box_dialog.h"
#include "api/model/rebuild_archive_reply.h"

#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "nx_ec/data/api_fwd.h"

class QLabel;

struct QnStorageSpaceReply;
struct QnStorageSpaceData;

namespace Ui {
    class ServerSettingsDialog;
}

class QnServerSettingsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT;

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();
signals:
    void rebuildArchiveDone();
public slots:
    virtual void accept() override;
    virtual void reject() override;

private:
    enum RebuildState {
        Invalid,
        Ready,
        Starting,
        InProgress,
        InProgressFastScan,
        Stopping
    };

    void updateFromResources();
    void submitToResources();

    void addTableItem(const QnStorageSpaceData &item);
    void setTableItems(const QList<QnStorageSpaceData> &items);
    QnStorageSpaceData tableItem(int row) const;
    QList<QnStorageSpaceData> tableItems() const;

    void setBottomLabelText(const QString &text);
    QString bottomLabelText() const;
    int dataRowCount() const;

    void updateRebuildUi(RebuildState newState, int progress = -1);
    void updateFailoverLabel();

private slots:
    void at_tableBottomLabel_linkActivated();
    void at_storagesTable_cellChanged(int row, int column);
    void at_storagesTable_contextMenuEvent(QObject *watched, QEvent *event);
    void at_pingButton_clicked();
    void at_rebuildButton_clicked();

    void at_archiveRebuildReply(int status, const QnRebuildArchiveReply& reply, int);
    void sendNextArchiveRequest();
    void at_updateRebuildInfo();

    void at_replyReceived(int status, const QnStorageSpaceReply &reply, int handle);
private:
    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;
    QList<QString> m_storageProtocols;
    QPointer<QLabel> m_tableBottomLabel;
    QAction *m_removeAction;

    bool m_hasStorageChanges;
    bool m_maxCamerasAdjusted;

    RebuildState m_rebuildState;
    ec2::ApiIdDataList m_storagesToRemove;
};

#endif // SERVER_SETTINGS_DIALOG_H
