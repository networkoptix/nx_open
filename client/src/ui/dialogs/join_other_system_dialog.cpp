#include "join_other_system_dialog.h"
#include "ui_join_other_system_dialog.h"

#include <QtCore/QUrl>

#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "ui/common/ui_resource_name.h"

QnJoinOtherSystemDialog::QnJoinOtherSystemDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnJoinOtherSystemDialog)
{
    ui->setupUi(this);
    ui->urlComboBox->lineEdit()->setPlaceholderText(tr("http(s)://host:port"));

    connect(ui->urlComboBox, SIGNAL(activated(int)), this, SLOT(at_urlComboBox_activated(int)));

    updateUi();
}

QnJoinOtherSystemDialog::~QnJoinOtherSystemDialog() {}

QUrl QnJoinOtherSystemDialog::url() const {
    return QUrl::fromUserInput(ui->urlComboBox->currentText());
}

QString QnJoinOtherSystemDialog::password() const {
    return ui->passwordEdit->text();
}

void QnJoinOtherSystemDialog::updateUi() {
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

    ui->urlComboBox->setCurrentText(QString());

    QString systemString;
    if (!qnCommon->localSystemName().isEmpty())
        systemString = lit(" (%1)").arg(qnCommon->localSystemName());
    ui->label->setText(tr("The specifien system will be joined to the current system%1.").arg(systemString));
}

void QnJoinOtherSystemDialog::at_urlComboBox_activated(int index) {
    if (index == -1)
        return;

    ui->urlComboBox->setCurrentText(ui->urlComboBox->itemData(index).toString());
}
