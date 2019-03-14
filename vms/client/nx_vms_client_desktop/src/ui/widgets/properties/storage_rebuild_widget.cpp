#include "storage_rebuild_widget.h"
#include "ui_storage_rebuild_widget.h"

#include <api/model/rebuild_archive_reply.h>

QnStorageRebuildWidget::QnStorageRebuildWidget(QWidget* parent) :
    base_type(parent),
    ui(new Ui::StorageRebuildWidget())
{
    ui->setupUi(this);
    loadData(QnStorageScanData(), false);

    connect(ui->stopButton, &QPushButton::clicked, this, [this]
    {
        ui->stopButton->setEnabled(false);
        emit cancelRequested();
    });
}

QnStorageRebuildWidget::~QnStorageRebuildWidget()
{
}

void QnStorageRebuildWidget::loadData(const QnStorageScanData& data, bool isBackup)
{
    QString placeholder = lit(" \t%p%");
    if (data.progress >= 0)
    {
        ui->progressBar->setValue(data.totalProgress * 100 + 0.5);
        switch (data.state)
        {
            case Qn::RebuildState_PartialScan:
                ui->progressBar->setFormat(isBackup
                    ? tr("Fast Backup Scan...") + placeholder
                    : tr("Fast Archive Scan...") + placeholder);
                break;
            case Qn::RebuildState_FullScan:
                ui->progressBar->setFormat(isBackup
                    ? tr("Reindexing Backup...") + placeholder
                    : tr("Reindexing Archive...") + placeholder);
                break;
            default:
                break;
        }
    }

    ui->stopButton->setEnabled(data.state == Qn::RebuildState_FullScan);
    setVisible(data.state == Qn::RebuildState_PartialScan || data.state == Qn::RebuildState_FullScan);
}
