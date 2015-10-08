#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"
#include <api/model/storage_space_reply.h>
#include <core/resource/client_storage_resource.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/style/warning_style.h>
#include "utils/common/event_processors.h"
#include <core/resource/media_server_resource.h>
#include "ui/models/storage_list_model.h"
#include "api/app_server_connection.h"

namespace {
    //setting free space to zero, since now client does not change this value, so it must keep current value
    const qint64 defaultReservedSpace = 0;  //5ll * 1024ll * 1024ll * 1024ll;

    class ArchiveSpaceItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        ArchiveSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {}

        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
            return new QnStorageSpaceSlider(parent);
        }

        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        }

        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override 
        {
        }
    };

} // anonymous namespace


QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageConfigWidget)
{
    ui->setupUi(this);
    
    setupGrid(ui->mainStoragesTable, &m_mainPool);
    setupGrid(ui->backupStoragesTable, &m_backupPool);

    connect(ui->addExtStorageToMainBtn, &QPushButton::clicked, this, [this]() {at_addExtStorage(true); });
    connect(ui->addExtStorageToBackupBtn, &QPushButton::clicked, this, [this]() {at_addExtStorage(false); });
    
    m_backupPool.model->setBackupRole(true);
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{

}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain)
{
    QnStorageListModel* model = addToMain ? m_mainPool.model : m_backupPool.model;

    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(model->modelData().storageProtocols);
    if(!dialog->exec())
        return;

    QnStorageSpaceData item = dialog->storage();
    if(!item.storageId.isNull())
        return;
    item.isUsedForWriting = true;
    item.isExternal = true;

    model->addModelData(item);
}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, StoragePool* storagePool)
{
    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    tableView->horizontalHeader()->setSectionResizeMode(ArchiveSpaceColumn, QHeaderView::ResizeToContents);
    tableView->setItemDelegateForColumn(ArchiveSpaceColumn, new ArchiveSpaceItemDelegate(this));
#else
#endif
    setWarningStyle(ui->storagesWarningLabel);
    ui->storagesWarningLabel->hide();

    //QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    //signalizer->setEventType(QEvent::ContextMenu);
    //tableView->installEventFilter(signalizer);

    storagePool->model = new QnStorageListModel();
    tableView->setModel(storagePool->model);

    //connect(signalizer, &QnAbstractEventSignalizer::activated,  this,   &QnStorageConfigWidget::at_storagesTable_contextMenuEvent);
    //connect(tableWidget,          &QTableWidget::cellChanged, this,   &QnStorageConfigWidget::at_storagesTable_cellChanged);
    connect(tableView,         &QTableView::clicked,               this,   &QnStorageConfigWidget::at_eventsGrid_clicked);
    tableView->setMouseTracking(true);
}

void QnStorageConfigWidget::updateFromSettings()
{
    sendStorageSpaceRequest();
}

void QnStorageConfigWidget::at_eventsGrid_clicked(const QModelIndex& index)
{
    bool isMain = index.model() == m_mainPool.model;

    if (index.column() == QnStorageListModel::ChangeGroupActionColumn)
    {
        QnStorageListModel* fromModel = m_mainPool.model;
        QnStorageListModel* toModel = m_backupPool.model;
        if (!isMain)
            qSwap(fromModel, toModel);

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        fromModel->removeRow(index.row());
        toModel->addModelData(record);
    }
    else if (index.column() == QnStorageListModel::RemoveActionColumn)
    {
        QnStorageListModel* model = isMain ? m_mainPool.model : m_backupPool.model;

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        if (record.isExternal)
            model->removeRow(index.row());
    }
}

void QnStorageConfigWidget::sendStorageSpaceRequest() {
    if (!m_server)
        return;
    m_mainPool.storageSpaceHandle = m_server->apiConnection()->getStorageSpaceAsync(true, this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
    m_backupPool.storageSpaceHandle = m_server->apiConnection()->getStorageSpaceAsync(false, this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
}

void QnStorageConfigWidget::at_replyReceived(int status, QnStorageSpaceReply reply, int handle) 
{
    if(status != 0) {
        //setBottomLabelText(tr("Could not load storages from server."));
        return;
    }
    
    qSort(reply.storages.begin(), reply.storages.end(), [](const QnStorageSpaceData &l, const QnStorageSpaceData &r) 
    {
        if (l.isExternal != r.isExternal)
            return l.isExternal < r.isExternal; 
        return l.url < r.url;
    });
    
    
    if (handle == m_mainPool.storageSpaceHandle)
        m_mainPool.model->setModelData(reply);
    else if (handle == m_backupPool.storageSpaceHandle)
        m_backupPool.model->setModelData(reply);
    
}

void QnStorageConfigWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server)
        return;

    if (m_server) {
        disconnect(m_server, nullptr, this, nullptr);
    }
    for (const auto &storage: m_storages)
        disconnect(storage, nullptr, this, nullptr);

    m_server = server;

    if (m_server) {
        connect(m_server,   &QnMediaServerResource::statusChanged,  this,   &QnStorageConfigWidget::updateRebuildInfo);
        connect(m_server,   &QnMediaServerResource::apiUrlChanged,  this,   &QnStorageConfigWidget::updateRebuildInfo);
        m_storages = m_server->getStorages();
        for (const auto& storage: m_storages)
            connect(storage, &QnResource::statusChanged,            this,   &QnStorageConfigWidget::updateRebuildInfo);
    }
}

void QnStorageConfigWidget::updateRebuildInfo() 
{
}

void QnStorageConfigWidget::processStorages(QnStorageResourceList& result, const QList<QnStorageSpaceData>& modifiedData, bool isBackupPool)
{
    for(const auto& storageData: modifiedData)
    {
        QnStorageResourcePtr storage = m_server->getStorageByUrl(storageData.url);
        if (storage) {
            if (storageData.isUsedForWriting != storage->isUsedForWriting() || isBackupPool != storage->isBackup()) 
            {
                storage->setUsedForWriting(storageData.isUsedForWriting);
                storage->setBackup(isBackupPool);
                result << storage;
            }
        }
        else {
            // create or remove new storage
            QnStorageResourcePtr storage(new QnClientStorageResource());
            if (!storageData.storageId.isNull())
                storage->setId(storageData.storageId);
            QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(lit("Storage"));
            if (resType)
                storage->setTypeId(resType->getId());
            storage->setName(QnUuid::createUuid().toString());
            storage->setParentId(m_server->getId());
            storage->setUrl(storageData.url);
            storage->setSpaceLimit(storageData.reservedSpace); //client does not change space limit anymore
            storage->setUsedForWriting(storageData.isUsedForWriting);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(isBackupPool);
            result << storage;
        }
    }
}

void QnStorageConfigWidget::submitToSettings() 
{
    if (isReadOnly())
        return;

    QnStorageResourceList storagesToUpdate;
    ec2::ApiIdDataList storagesToRemove;

    processStorages(storagesToUpdate, m_mainPool.model->modelData().storages, false);
    processStorages(storagesToUpdate, m_backupPool.model->modelData().storages, true);

    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_mainPool.model->modelData().storages + m_backupPool.model->modelData().storages)
        newIdList << storageData.storageId;
    for (const auto& storage: m_storages) {
        if (!newIdList.contains(storage->getId()))
            storagesToRemove.push_back(storage->getId());
    }

    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    if (!conn)
        return;

    //TODO: #GDM SafeMode
    if (!storagesToUpdate.isEmpty())
        conn->getMediaServerManager()->saveStorages(storagesToUpdate, this, []{});

    if (!storagesToRemove.empty())
        conn->getMediaServerManager()->removeStorages(storagesToRemove, this, []{});
}
