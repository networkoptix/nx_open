#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtCore/QUrl>

#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "ui/common/ui_resource_name.h"

QnMergeSystemsDialog::QnMergeSystemsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnMergeSystemsDialog)
{
    ui->setupUi(this);
    ui->urlComboBox->lineEdit()->setPlaceholderText(tr("http(s)://host:port"));

    connect(ui->urlComboBox,            SIGNAL(activated(int)),     this,   SLOT(at_urlComboBox_activated(int)));
    connect(ui->testConnectionButton,   &QPushButton::clicked,      this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);

    updateKnownSystems();
    //    ui->currentSystemLabel->setText(tr("You are about to merge the current system %1 with the system %2").arg(qnCommon->localSystemName()).arg(systemString));
}

QnMergeSystemsDialog::~QnMergeSystemsDialog() {}

QUrl QnMergeSystemsDialog::url() const {
    /* filter unnecessary information from the URL */
    QUrl enteredUrl = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QUrl url;
    url.setScheme(enteredUrl.scheme());
    url.setHost(enteredUrl.host());
    url.setPort(enteredUrl.port());
    return url;
}

QString QnMergeSystemsDialog::password() const {
    return ui->passwordEdit->text();
}

void QnMergeSystemsDialog::updateKnownSystems() {
    ui->urlComboBox->clear();

    foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QString url = server->getApiUrl();
        QString label = getResourceName(server);
        QString systemName = server->getSystemName();
        if (!systemName.isEmpty())
            label += lit(" (%1)").arg(systemName);

        ui->urlComboBox->addItem(label, url);
    }
}

void QnMergeSystemsDialog::at_urlComboBox_activated(int index) {
    if (index == -1)
        return;

    ui->urlComboBox->setCurrentText(ui->urlComboBox->itemData(index).toString());
}

void QnMergeSystemsDialog::at_testConnectionButton_clicked() {

}
