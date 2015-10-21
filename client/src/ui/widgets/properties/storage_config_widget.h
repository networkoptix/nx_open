#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/model/recording_stats_reply.h>
#include <api/model/storage_space_reply.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>
#include "api/model/rebuild_archive_reply.h"
#include "core/resource/media_server_user_attributes.h"
#include "ui/models/backup_settings_model.h"
#include "api/model/backup_status_reply.h"

namespace Ui {
    class StorageConfigWidget;
}

class QnStorageListModel;
class QnStorageConfigWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware 
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnStorageConfigWidget(QWidget* parent = 0);
    virtual ~QnStorageConfigWidget();

    virtual void updateFromSettings() override;

    void setServer(const QnMediaServerResourcePtr &server);
    virtual void submitToSettings() override;
private:
    void updateRebuildInfo();
    void updateBackupInfo();
    void at_eventsGrid_clicked(const QModelIndex& index);
    void sendNextArchiveRequest(bool isMain);
    void updateRebuildUi(const QnStorageScanData& reply, bool isMainPool);
    void updateBackupUi(const QnBackupStatusData& reply);
    void updateColumnWidth();
    int getColWidth(const QAbstractItemModel* model, int col);
    bool hasUnsavedChanges() const;

    void startRebuid(bool isMain);
    void cancelRebuild(bool isMain);

    void startBackup();
    void cancelBackup();

private slots:
    void at_replyReceived(int status, QnStorageSpaceReply reply, int);
    void sendStorageSpaceRequest();
    void at_addExtStorage(bool addToMain);
    
    void at_openBackupSchedule_clicked();
    void at_openBackupSettings_clicked();
    void at_archiveRebuildReply(int status, const QnStorageScanData& reply, int handle);
    void at_backupStatusReply(int status, const QnBackupStatusData& reply, int handle);
    
    void sendNextBackupStatusRequest();
    void at_backupTypeComboBoxChange(int index);
private:
    Ui::StorageConfigWidget* ui;
    QnMediaServerResourcePtr m_server;
    QnStorageResourceList m_storages;
    struct StoragePool
    {
        StoragePool();
        int storageSpaceHandle;
        int rebuildHandle;
        QScopedPointer<QnStorageListModel> model;
        QnStorageScanData prevRebuildState;
    };

    StoragePool m_mainPool;
    StoragePool m_backupPool;
    QnMediaServerUserAttributes m_serverUserAttrs;
    BackupSettingsDataList m_cameraBackupSettings;
private:
    void setupGrid(QTableView* tableView, StoragePool* storagePool);
    void processStorages(QnStorageResourceList& result, const QList<QnStorageSpaceData>& newData, bool isBackupPool) const;
};
