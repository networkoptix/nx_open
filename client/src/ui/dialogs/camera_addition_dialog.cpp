#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <functional>

#include <QtCore/QUuid>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QSpinBox>

#include <utils/common/counter.h>

#include <core/resource/storage_resource.h>
#include <core/resource/video_server_resource.h>

static const qint64 MIN_RECORD_FREE_SPACE = 1000ll * 1000ll * 1000ll * 5;

namespace {
    const int defaultSpaceLimitGb = 5;
    const qint64 BILLION = 1000000000LL;

    class StorageSettingsItemEditorFactory: public QItemEditorFactory {
    public:
        virtual QWidget *createEditor(QVariant::Type type, QWidget *parent) const override {
            QWidget *result = QItemEditorFactory::createEditor(type, parent);

            if(QSpinBox *spinBox = dynamic_cast<QSpinBox *>(result))
                spinBox->setRange(0, 10000); /* That's for space limit. */

            return result;
        }
    };

} // anonymous namespace

QnCameraAdditionDialog::QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(server)
{
    ui->setupUi(this);
/*    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->storagesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setVisible(true); */
    /* Qt designer does not save this flag (probably a bug in Qt designer). */

    QStyledItemDelegate *itemDelegate = new QStyledItemDelegate(this);
    itemDelegate->setItemEditorFactory(new StorageSettingsItemEditorFactory());
    ui->camerasTable->setItemDelegate(itemDelegate);

    setButtonBox(ui->buttonBox);

    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLabel, SLOT(setVisible(bool)));
    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLineEdit, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLineEdit, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLineEdit, SLOT(setVisible(bool)));

    ui->startIPLabel->setVisible(false);
    ui->startIPLineEdit->setVisible(false);
    ui->endIPLabel->setVisible(false);
    ui->endIPLineEdit->setVisible(false);
    ui->scanProgressBar->setVisible(false);
    ui->camerasGroupBox->setVisible(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));


/*    connect(ui->storageAddButton,       SIGNAL(clicked()),              this,   SLOT(at_storageAddButton_clicked()));
    connect(ui->storageRemoveButton,    SIGNAL(clicked()),              this,   SLOT(at_storageRemoveButton_clicked()));
    connect(ui->storagesTable,          SIGNAL(cellChanged(int, int)),  this,   SLOT(at_storagesTable_cellChanged(int, int)));*/
}

QnCameraAdditionDialog::~QnCameraAdditionDialog() {
    return;
}

int QnCameraAdditionDialog::addTableRow(const QByteArray &data) {
    int row = ui->camerasTable->rowCount();
    ui->camerasTable->insertRow(row);

    QTableWidgetItem *urlItem = new QTableWidgetItem(QString::fromLatin1(data));
    QTableWidgetItem *spaceItem = new QTableWidgetItem();

    ui->camerasTable->setItem(row, 0, urlItem);
    ui->camerasTable->setItem(row, 1, spaceItem);

    updateSpaceLimitCell(row, true);

    return row;
}

void QnCameraAdditionDialog::setTableStorages(const QnAbstractStorageResourceList &storages) {
/*    ui->storagesTable->setRowCount(0);
    ui->storagesTable->setColumnCount(2);

    foreach (const QnAbstractStorageResourcePtr &storage, storages)
        addTableRow(storage->getId().toInt(), storage->getUrl(), storage->getSpaceLimit() / BILLION);*/
}

QnAbstractStorageResourceList QnCameraAdditionDialog::tableStorages() const {
/*    QnAbstractStorageResourceList result;

    int rowCount = ui->storagesTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        int id = ui->storagesTable->item(row, 0)->data(Qt::UserRole).toInt();
        QString path = ui->storagesTable->item(row, 0)->text();
        int spaceLimit = ui->storagesTable->item(row, 1)->data(Qt::DisplayRole).toInt();

        QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
        if (id)
            storage->setId(id);
        storage->setName(QUuid::createUuid().toString());
        storage->setParentId(m_server->getId());
        storage->setUrl(path.trimmed());
        storage->setSpaceLimit(spaceLimit * BILLION);

        result.append(storage);
    }

    return result;*/
}

bool QnCameraAdditionDialog::validateStorages(const QnAbstractStorageResourceList &storages, QString *errorString) {
    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        if (storage->getUrl().isEmpty()) {
            if(errorString)
                *errorString = tr("Storage path must not be empty.");
            return false;
        }

        if (storage->getSpaceLimit() < 0) {
            if(errorString)
                *errorString = tr("Space limit must be a non-negative integer.");
            return false;
        }
    }

    QScopedPointer<QnCounter> counter(new QnCounter(storages.size()));
    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());
    connect(counter.data(), SIGNAL(reachedZero()), eventLoop.data(), SLOT(quit()));

    QScopedPointer<detail::CheckCamerasFoundReplyProcessor> processor(new detail::CheckCamerasFoundReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived(int, qint64, qint64, int)), counter.data(), SLOT(decrement()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    QHash<int, QnAbstractStorageResourcePtr> storageByHandle;
    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        int handle = serverConnection->asyncGetFreeSpace(storage->getUrl(), processor.data(), SLOT(processReply(int, qint64, qint64, int)));
        storageByHandle[handle] = storage;
    }

    eventLoop->exec();

    detail::CamerasFoundInfoMap freeSpaceMap = processor->freeSpaceInfo();
    for (detail::CamerasFoundInfoMap::const_iterator itr = freeSpaceMap.constBegin(); itr != freeSpaceMap.constEnd(); ++itr)
    {
        QnAbstractStorageResourcePtr storage = storageByHandle.value(itr.key());
        if (!storage)
            continue;

        qint64 availableSpace = itr.value().freeSpace + itr.value().usedSpace - storage->getSpaceLimit();
        if (itr.value().errorCode == -1)
        {
            QMessageBox::warning(this, tr("Invalid storage path"), tr("Storage path '%1' is invalid or is not accessible for writing.").arg(storage->getUrl()));
            return false;
        }
        if (itr.value().errorCode == -2)
        {
            QMessageBox::critical(this, tr("Can't verify storage path"), tr("Cannot verify storage path '%1'. Cannot establish connection to the media server.").arg(storage->getUrl()));
            return false;
        }
        else if (availableSpace < 0)
        {
            QMessageBox::critical(this, tr("Not enough disk space"),
                tr("Storage '1'\nYou have less storage space available than reserved free space value. Additional 2Gb are required."));
            return false;
        }
        else if (availableSpace < MIN_RECORD_FREE_SPACE)
        {
            QMessageBox::warning(this, tr("Low space for archive"),
                tr("Storage '%1'\nYou have only 2Gb left for video archive.")
                .arg(storage->getUrl()));
        }
    }

    return true;
}

void QnCameraAdditionDialog::updateSpaceLimitCell(int row, bool force) {

        /* Open/close editor. */
   /*     if(newSupportsSpaceLimit) {
            ui->storagesTable->openPersistentEditor(spaceItem);
        } else {
            ui->storagesTable->closePersistentEditor(spaceItem);
        }*/
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnCameraAdditionDialog::at_scanButton_clicked(){
    ui->buttonBox->setEnabled(false);
    ui->scanProgressBar->setVisible(true);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::CheckCamerasFoundReplyProcessor> processor(new detail::CheckCamerasFoundReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    int handle = serverConnection->asyncGetCameraAddition(processor.data(), SLOT(processReply(int, const QByteArray &)));
    Q_UNUSED(handle)

    eventLoop->exec();

    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);
    ui->camerasGroupBox->setVisible(true);
    addTableRow(processor->getData());
}
