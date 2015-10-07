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

namespace {
    //setting free space to zero, since now client does not change this value, so it must keep current value
    const qint64 defaultReservedSpace = 0;  //5ll * 1024ll * 1024ll * 1024ll;

    const qint64 bytesInMiB = 1024 * 1024;

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
    ui(new Ui::StorageConfigWidget),
    m_hasStorageChanges(false)
{
    ui->setupUi(this);
    
    setupGrid(ui->mainStoragesTable, &m_mainPool);
    setupGrid(ui->backupStoragesTable, &m_backupPool);
    
    m_backupPool.model->setBackupRole(true);
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{

}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, StoragePool* storagePool)
{
    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    //tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    tableView->horizontalHeader()->setSectionResizeMode(ArchiveSpaceColumn, QHeaderView::ResizeToContents);
    tableView->setItemDelegateForColumn(ArchiveSpaceColumn, new ArchiveSpaceItemDelegate(this));
#else
#endif
    setWarningStyle(ui->storagesWarningLabel);
    ui->storagesWarningLabel->hide();

    QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    signalizer->setEventType(QEvent::ContextMenu);
    tableView->installEventFilter(signalizer);

    storagePool->model = new QnStorageListModel();
    tableView->setModel(storagePool->model);

    //connect(signalizer, &QnAbstractEventSignalizer::activated,  this,   &QnStorageConfigWidget::at_storagesTable_contextMenuEvent);
    //connect(tableWidget,          &QTableWidget::cellChanged, this,   &QnStorageConfigWidget::at_storagesTable_cellChanged);
}

void QnStorageConfigWidget::updateFromSettings()
{
    sendStorageSpaceRequest();
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
