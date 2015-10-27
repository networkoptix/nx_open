#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/model/api_model_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/media_server_user_attributes.h>

#include <server/server_storage_manager_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

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

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    void updateStoragesInfo();
    void updateRebuildInfo();
    void updateBackupInfo();
    void at_eventsGrid_clicked(const QModelIndex& index);

    void updateStoragesUi(QnServerStoragesPool pool, const QnStorageSpaceDataList &storages);
    void updateRebuildUi(QnServerStoragesPool pool, const QnStorageScanData& reply);
    void updateBackupUi(const QnBackupStatusData& reply);

    void updateColumnWidth();
    int getColWidth(const QAbstractItemModel* model, int col);

    void startRebuid(bool isMain);
    void cancelRebuild(bool isMain);

    void startBackup();
    void cancelBackup();

private slots:
    
    void at_serverStorageSpaceChanged(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageSpaceDataList &storages);
    void at_serverRebuildStatusChanged(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageScanData &status);
    void at_serverBackupStatusChanged(const QnMediaServerResourcePtr &server, const QnBackupStatusData &status);
    
    void at_serverRebuildArchiveFinished(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    void at_serverBackupFinished(const QnMediaServerResourcePtr &server);

    void at_addExtStorage(bool addToMain);
    
    void at_openBackupSchedule_clicked();
    
    void at_backupTypeComboBoxChange(int index);
private:
    QScopedPointer<Ui::StorageConfigWidget> ui;
    QnMediaServerResourcePtr m_server;
    QnStorageResourceList m_storages;
    struct StoragePool
    {
        StoragePool();
        QScopedPointer<QnStorageListModel> model;
        bool rebuildCancelled;
        bool hasStorageChanges;
    };

    StoragePool m_mainPool;
    StoragePool m_backupPool;
    QnServerBackupSchedule m_backupSchedule;

    bool m_backupCancelled;
private:
    void setupGrid(QTableView* tableView, StoragePool* storagePool);
    void processStorages(QnStorageResourceList& result, const QList<QnStorageSpaceData>& newData, bool isBackupPool) const;
};
