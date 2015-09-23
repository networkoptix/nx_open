#include "videowall_settings_dialog.h"
#include "ui_videowall_settings_dialog.h"

#include <api/app_server_connection.h>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <platform/platform_abstraction.h>

#include "version.h"

QnVideowallSettingsDialog::QnVideowallSettingsDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnVideowallSettingsDialog)
{
    ui->setupUi(this);
}

QnVideowallSettingsDialog::~QnVideowallSettingsDialog() {}

void QnVideowallSettingsDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    ui->autoRunCheckBox->setChecked(videowall->isAutorun());

    bool shortcutsSupported = qnPlatform->shortcuts()->supported();
    ui->shortcutCheckbox->setVisible(shortcutsSupported);

    if (!shortcutsSupported)
        return;

    bool exists = shortcutExists(videowall);
    ui->shortcutCheckbox->setChecked(exists);
    if (!exists && !canStartVideowall(videowall))
        ui->shortcutCheckbox->setEnabled(false);
}

void QnVideowallSettingsDialog::submitToResource(const QnVideoWallResourcePtr &videowall) {
    videowall->setAutorun(ui->autoRunCheckBox->isChecked());

    if (!qnPlatform->shortcuts()->supported())
        return;

    if (!ui->shortcutCheckbox->isChecked()) 
        deleteShortcut(videowall);
    else if (canStartVideowall(videowall))
        createShortcut(videowall);
}

QString QnVideowallSettingsDialog::shortcutPath() {
    QString result = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (result.isEmpty())
        result = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return result;
}

bool QnVideowallSettingsDialog::shortcutExists(const QnVideoWallResourcePtr &videowall) const {
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    return qnPlatform->shortcuts()->shortcutExists(destinationPath, videowall->getName());
}

bool QnVideowallSettingsDialog::createShortcut(const QnVideoWallResourcePtr &videowall) {
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    QStringList arguments;
    arguments << lit("--videowall");
    arguments << videowall->getId().toString();

    QUrl url = QnAppServerConnectionFactory::url();
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << lit("--auth");
    arguments << QString::fromUtf8(url.toEncoded());

    return qnPlatform->shortcuts()->createShortcut(qApp->applicationFilePath(), destinationPath, videowall->getName(), arguments, IDI_ICON_VIDEOWALL);
}

bool QnVideowallSettingsDialog::deleteShortcut(const QnVideoWallResourcePtr &videowall) {
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return true;
    return qnPlatform->shortcuts()->deleteShortcut(destinationPath, videowall->getName());
}

//TODO: #GDM duplicate code
bool QnVideowallSettingsDialog::canStartVideowall(const QnVideoWallResourcePtr &videowall) const {
    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        qWarning() << "Warning: pc UUID is null, cannot start Video Wall on this pc";
        return false;
    }

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        if (item.pcUuid != pcUuid || item.runtimeStatus.online)
            continue;
        return true;
    }
    return false;
}
