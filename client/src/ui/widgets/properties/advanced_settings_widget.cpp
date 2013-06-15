#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"
#include "ui/style/skin.h"

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::AdvancedSettingsWidget)
{
    ui->setupUi(this);
    ui->iconLabel->setPixmap(qnSkin->pixmap("warning.png"));
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{

}
