#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/model/api_model_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/media_server_user_attributes.h>

#include <server/server_storage_manager_fwd.h>

#include <ui/models/storage_model_info.h>
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

    virtual void loadDataToUi() override;

    void setServer(const QnMediaServerResourcePtr &server);
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;
    virtual void retranslateUi() override;
protected:

    virtual void setReadOnlyInternal(bool readOnly) override;
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private:
    /** Load initial storages data from resource pool. */
    void loadStoragesFromResources();

    void updateRebuildInfo();
    void updateBackupInfo();
    void at_eventsGrid_clicked(const QModelIndex& index);

    void updateRebuildUi(QnServerStoragesPool pool, const QnStorageScanData& reply);
    void updateBackupUi(const QnBackupStatusData& reply);
    void updateCamerasLabel();

    void updateColumnWidth();
    int getColWidth(int col);

    void startRebuid(bool isMain);
    void cancelRebuild(bool isMain);

    void startBackup();
    void cancelBackup();

    /** Check if backup can be started right now - and show additional info if not. */
    bool canStartBackup(const QnBackupStatusData& data, QString *info);

    QString backupPositionToString(qint64 backupTimeMs);

private slots:

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
    QScopedPointer<QnStorageListModel> m_model;
    QTimer* m_updateStatusTimer;

    struct StoragePool
    {
        StoragePool();
        bool rebuildCancelled;
    };

    StoragePool m_mainPool;
    StoragePool m_backupPool;
    QnServerBackupSchedule m_backupSchedule;

    bool m_backupCancelled;
    bool m_updating;
private:
    void setupGrid(QTableView* tableView, bool isMainPool);
    void applyStoragesChanges(QnStorageResourceList& result, const QnStorageModelInfoList &storages) const;
    bool hasStoragesChanges(const QnStorageModelInfoList &storages) const;
    void updateBackupWidgetsVisibility();
};
