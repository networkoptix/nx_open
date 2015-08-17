#pragma once

#include <QtWidgets/QWidget>

#include <api/model/rebuild_archive_reply.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

struct QnStorageSpaceReply;
struct QnStorageSpaceData;

namespace Ui {
    class ServerSettingsWidget;
}

class QnServerSettingsWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnServerSettingsWidget(const QnMediaServerResourcePtr &server, QWidget* parent = 0);
    virtual ~QnServerSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

private:
    void addTableItem(const QnStorageSpaceData &item);
    void setTableItems(const QList<QnStorageSpaceData> &items);
    QnStorageSpaceData tableItem(int row) const;
    QList<QnStorageSpaceData> tableItems() const;

    void setBottomLabelText(const QString &text);
    QString bottomLabelText() const;
    int dataRowCount() const;

    void updateRebuildUi(const QnStorageScanData& reply);
    void updateFailoverLabel();
    
private slots:
    void at_tableBottomLabel_linkActivated();
    void at_storagesTable_cellChanged(int row, int column);
    void at_storagesTable_contextMenuEvent(QObject *watched, QEvent *event);
    void at_pingButton_clicked();
    void at_rebuildButton_clicked();

    void at_archiveRebuildReply(int status, const QnStorageScanData& reply, int);
    void sendNextArchiveRequest();
    void at_updateRebuildInfo();

    void sendStorageSpaceRequest();
    void at_replyReceived(int status, const QnStorageSpaceReply &reply, int handle);

private:
    QScopedPointer<Ui::ServerSettingsWidget> ui;

    QnMediaServerResourcePtr m_server;

    QList<QString> m_storageProtocols;
    QPointer<QLabel> m_tableBottomLabel;
    QVector<bool> m_initialStorageCheckStates;
    QAction *m_removeAction;

    bool m_hasStorageChanges;
    bool m_maxCamerasAdjusted;

    QnStorageScanData m_rebuildState;
    ec2::ApiIdDataList m_storagesToRemove;
    bool m_rebuildWasCanceled;
};
