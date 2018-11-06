#include "custom_settings_widget.h"
#include "ui_custom_settings_widget.h"

#include <nx/vms/client/desktop/resource_properties/camera/camera_advanced_param_widgets_manager.h>

namespace nx::vms::client::desktop {

struct CustomSettingsWidget::Private
{
    QScopedPointer<CameraAdvancedParamWidgetsManager> manager;

    void init(QTreeWidget* groupWidget, QStackedWidget* contentsWidget)
    {
        manager.reset(new CameraAdvancedParamWidgetsManager(groupWidget, contentsWidget));
    }

};


CustomSettingsWidget::CustomSettingsWidget(QWidget* parent /*= NULL*/):
    base_type(parent),
    d(new Private()),
    ui(new Ui::CustomSettingsWidget)
{
    ui->setupUi(this);

    QList<int> sizes = ui->splitter->sizes();
    sizes[0] = 200;
    sizes[1] = 400;
    ui->splitter->setSizes(sizes);

    d->init(ui->groupsWidget, ui->contentsWidget);
}

CustomSettingsWidget::~CustomSettingsWidget()
{}


void CustomSettingsWidget::loadManifest(const QnCameraAdvancedParams& manifest)
{
    d->manager->displayParams(manifest);
}

} // namespace nx::vms::client::desktop
