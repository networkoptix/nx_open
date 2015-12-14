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
    if (data.progress >= 0) {
        ui->progressBar->setValue(data.totalProgress * 100 + 0.5);
        switch (data.state)
        {
        case Qn::RebuildState_PartialScan:
            ui->progressBar->setFormat(tr("Fast Archive Scan - %p%", "%p is a placeholder for percent value, must be kept."));
            break;
        case Qn::RebuildState_FullScan:
            ui->progressBar->setFormat(tr("Rebuilding Archive Index - %p%", "%p is a placeholder for percent value, must be kept."));
            break;
        default:
            break;
        }
    }


    ui->stopButton->setEnabled(data.state == Qn::RebuildState_FullScan);
    setVisible(data.state == Qn::RebuildState_PartialScan || data.state == Qn::RebuildState_FullScan);
}
