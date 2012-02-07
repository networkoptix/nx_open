#include "logindialog.h"
#include "ui_serversettingsdialog.h"

#include <set>

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "serversettingsdialog.h"

#include "settings.h"

#define BILLION 1000000000LL

ServerSettingsDialog::ServerSettingsDialog(QWidget *parent, QnVideoServerResourcePtr server) :
    QDialog(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_connection (QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // qt designed do not save this flag (probably because of a bug in qt designer)
    ui->storagesTable->horizontalHeader()->setVisible(true);

    initView();
}

ServerSettingsDialog::~ServerSettingsDialog()
{
}

void ServerSettingsDialog::requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle)
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
    QnStorageList storages;
    int rowCount = ui->storagesTable->rowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        QString name = ui->storagesTable->item(row, 0)->text();
        QString path = ui->storagesTable->item(row, 1)->text();
        int spaceLimit = ui->storagesTable->item(row, 2)->text().toInt();

        QnStoragePtr storage(new QnStorage());
        storage->setName(name.trimmed());
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

void ServerSettingsDialog::initView()
{
    ui->nameEdit->setText(m_server->getName());
    ui->ipAddressEdit->setText(m_server->getUrl());
    ui->apiPortEdit->setText(QString::number(QUrl(m_server->getApiUrl()).port()));
    ui->rtspPortEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    foreach (const QnStoragePtr& storage, m_server->getStorages())
    {
        int row = ui->storagesTable->rowCount();
        ui->storagesTable->insertRow(row);
        ui->storagesTable->setItem(row, 0, new QTableWidgetItem(storage->getName(), QTableWidgetItem::Type));
        ui->storagesTable->setItem(row, 1, new QTableWidgetItem(storage->getUrl(), QTableWidgetItem::Type));
        ui->storagesTable->setItem(row, 2, new QTableWidgetItem(QString::number(storage->getSpaceLimit() / (BILLION)), QTableWidgetItem::Type));
    }
}

void ServerSettingsDialog::buttonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        saveToModel();
        save();
    }
}

void ServerSettingsDialog::save()
{
    m_connection->saveAsync(m_server, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}

void ServerSettingsDialog::addClicked()
{
    ui->storagesTable->insertRow(ui->storagesTable->rowCount());
}

void ServerSettingsDialog::removeClicked()
{
    std::set<int> rows;

    foreach (QTableWidgetItem * item, ui->storagesTable->selectedItems())
        rows.insert(item->row());

    for (std::set<int>::const_reverse_iterator ci = rows.rbegin(); ci != rows.rend(); ++ci)
        ui->storagesTable->removeRow(*ci);
}

bool ServerSettingsDialog::validateStorages(const QnStorageList& storages, QString& errorString)
{
    QSet<QString> names;

    foreach (const QnStoragePtr& storage, storages)
    {
        if (storage->getName().isEmpty())
        {
            errorString = "Storage name should not be empty. Please select any name you like.";
            return false;
        }

        if (storage->getUrl().isEmpty())
        {
            errorString = "Storage name should not be empty. Please select any name you like.";
            return false;
        }

        if (names.contains(storage->getName()))
        {
            errorString = "Storage name should be unique";
            return false;
        }
        names.insert(storage->getName());

        if (storage->getSpaceLimit() <= 0)
        {
            errorString = "Space Limit should be positive integer";
            return false;
        }
    }

    return true;
}
