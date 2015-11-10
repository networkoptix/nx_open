#include "storage_rebuild_widget.h"
#include "ui_storage_rebuild_widget.h"

#include <api/model/rebuild_archive_reply.h>

QnStorageRebuildWidget::QnStorageRebuildWidget( QWidget *parent )
    : base_type(parent)
    , ui(new Ui::StorageRebuildWidget())
{
    ui->setupUi(this);
    loadData(QnStorageScanData());

    connect(ui->stopButton, &QPushButton::clicked, this, [this]{
        ui->stopButton->setEnabled(false);
        emit cancelRequested();
    });
}

QnStorageRebuildWidget::~QnStorageRebuildWidget()
{}

void QnStorageRebuildWidget::loadData( const QnStorageScanData &data ) {
    ui->statusLabel->setText(tr("Rebuild archive index for storage '%1' is in progress").arg(data.path));

    if (data.progress >= 0)
        ui->progressBar->setValue(data.progress * 100 + 0.5);

    ui->stopButton->setEnabled(data.state == Qn::RebuildState_FullScan);
    setVisible(data.state == Qn::RebuildState_FullScan);
}
