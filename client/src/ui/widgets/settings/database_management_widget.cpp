#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include "utils/settings.h"

QnDatabaseManagementWidget::QnDatabaseManagementWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::DatabaseManagementWidget())
{
    ui->setupUi(this);
    
    connect(ui->backupButton, SIGNAL(clicked()), this, SLOT(at_backupButton_clicked()));
    connect(ui->restoreButton, SIGNAL(clicked()), this, SLOT(at_restoreButton_clicked()));
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget() {
    return;
}

void QnDatabaseManagementWidget::at_backupButton_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.bin)"), &fileName);
    if(fileName.isEmpty())
        return;

    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());
    
}

void QnDatabaseManagementWidget::at_restoreButton_clicked() {


}

