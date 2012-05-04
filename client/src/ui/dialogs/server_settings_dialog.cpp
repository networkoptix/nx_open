#include "login_dialog.h"
#include "ui_server_settings_dialog.h"

#include <set>

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "server_settings_dialog.h"

#include "utils/settings.h"

#define BILLION 1000000000LL

ServerSettingsDialog::ServerSettingsDialog(QnVideoServerResourcePtr server, QWidget *parent):
    QDialog(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_connection (QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->storagesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    // qt designed do not save this flag (probably because of a bug in qt designer)
    ui->storagesTable->horizontalHeader()->setVisible(true);

    initView();
}

ServerSettingsDialog::~ServerSettingsDialog()
{
}

void ServerSettingsDialog::requestFinished(int status, const QByteArray &/*errorString*/, QnResourceList /*resources*/, int /*handle*/)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save server configuration", "Can't save server. Please try again later.");
        ui->buttonBox->setEnabled(true);
    }
}

void ServerSettingsDialog::accept()
{
    ui->buttonBox->setEnabled(false);

    if (saveToModel())
        save();
    else
        ui->buttonBox->setEnabled(true);
}


void ServerSettingsDialog::reject()
{
    QDialog::reject();
}

bool ServerSettingsDialog::saveToModel()
{
    QnAbstractStorageResourceList storages;
    int rowCount = ui->storagesTable->rowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        QString path = ui->storagesTable->item(row, 0) ? ui->storagesTable->item(row, 0)->text() : "";
        int spaceLimit = static_cast<QSpinBox*>(ui->storagesTable->cellWidget(row, 1))->value();

        QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
        storage->setName(QUuid::createUuid().toString());
        storage->setParentId(m_server->getId());
        storage->setUrl(path.trimmed());
        storage->setSpaceLimit(spaceLimit * BILLION);

        storages.append(storage);
    }

    QString errorString;
    if (!validateStorages(storages, errorString))
    {
        QMessageBox mbox(QMessageBox::Warning, tr("Warning"), errorString, QMessageBox::Ok, this, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
        mbox.exec();
        return false;
    }

    m_server->setStorages(storages);
    m_server->setName(ui->nameEdit->text());

    QUrl url(m_server->getUrl());
    url.setPort(ui->rtspPortEdit->text().toInt());
    m_server->setUrl(url.toString());

    QUrl apiUrl(m_server->getApiUrl());
    apiUrl.setPort(ui->apiPortEdit->text().toInt());
    m_server->setApiUrl(apiUrl.toString());

    return true;
}

QSpinBox* ServerSettingsDialog::createSpinCellWidget(QWidget* parent)
{
    QSpinBox* spinBox = new QSpinBox(parent);

    spinBox->setValue(5);
    spinBox->setMinimum(0);
    spinBox->setMaximum(100000);

    return spinBox;
}

void ServerSettingsDialog::initView()
{
    ui->nameEdit->setText(m_server->getName());
    ui->ipAddressEdit->setText(QUrl(m_server->getUrl()).host());
    ui->apiPortEdit->setText(QString::number(QUrl(m_server->getApiUrl()).port()));
    ui->rtspPortEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    foreach (const QnAbstractStorageResourcePtr& storage, m_server->getStorages())
    {
        int row = ui->storagesTable->rowCount();
        ui->storagesTable->insertRow(row);
        ui->storagesTable->setCellWidget (row, 1, createSpinCellWidget(ui->storagesTable));
        ui->storagesTable->setItem(row, 0, new QTableWidgetItem(storage->getUrl(), QTableWidgetItem::Type));
        static_cast<QSpinBox*>(ui->storagesTable->cellWidget(row,1))->setValue(storage->getSpaceLimit() / (BILLION));
        // setItem(row, 1, new QTableWidgetItem(QString::number(storage->getSpaceLimit() / (BILLION)), QTableWidgetItem::Type));
    }
}

void ServerSettingsDialog::save()
{
    m_connection->saveAsync(m_server, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}

void ServerSettingsDialog::addClicked()
{
    int row = ui->storagesTable->rowCount();
    ui->storagesTable->insertRow(row);
    ui->storagesTable->setCellWidget (row, 1, createSpinCellWidget(ui->storagesTable));
}

void ServerSettingsDialog::removeClicked()
{
    std::set<int> rows;

    foreach (QModelIndex index, ui->storagesTable->selectionModel()->selectedRows())
        rows.insert(index.row());

    for (std::set<int>::const_reverse_iterator ci = rows.rbegin(); ci != rows.rend(); ++ci)
        ui->storagesTable->removeRow(*ci);
}

bool ServerSettingsDialog::validateStorages(const QnAbstractStorageResourceList& storages, QString& errorString)
{
    foreach (const QnAbstractStorageResourcePtr& storage, storages)
    {
        if (storage->getUrl().isEmpty())
        {
            errorString = "Storage path should not be empty.";
            return false;
        }

        if (storage->getSpaceLimit() < 0)
        {
            errorString = "Space Limit should be non-negative integer";
            return false;
        }
    }

    return true;
}
