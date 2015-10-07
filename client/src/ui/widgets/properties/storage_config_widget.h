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

namespace Ui {
    class StorageConfigWidget;
}

class QnStorageConfigWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware 
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnStorageConfigWidget(QWidget* parent = 0);
    virtual ~QnStorageConfigWidget();

    virtual void updateFromSettings() override;

    void setServer(const QnMediaServerResourcePtr &server);
    bool hasChanges() const { return m_hasStorageChanges; }
private:
    void setupGrid(QTableWidget* tableWidget);
    void updateRebuildInfo();
private slots:
    void at_replyReceived(int status, const QnStorageSpaceReply &reply, int);
    void sendStorageSpaceRequest();
private:
    Ui::StorageConfigWidget* ui;
    bool m_hasStorageChanges;
    QnMediaServerResourcePtr m_server;
    QnStorageResourceList m_storages;
    
    struct RequestInfo
    {
        RequestInfo(): storageSpaceHandle(-1) {}
        int storageSpaceHandle;
    };

    RequestInfo m_mainPool;
    RequestInfo m_backupPool;
};
