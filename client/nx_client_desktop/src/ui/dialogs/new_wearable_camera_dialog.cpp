#include "new_wearable_camera_dialog.h"
#include "ui_new_wearable_camera_dialog.h"

#include <algorithm>

#include <nx/utils/string.h>
#include <ui/models/resource/resource_list_model.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>


QnNewWearableCameraDialog::QnNewWearableCameraDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::NewWearableCameraDialog)
{
    ui->setupUi(this);

    // TODO: #wearable help topics?
    //setHelpTopic(, Qn::);

    /* Set up combobox. */
    QnResourceList servers = resourcePool()->getAllServers(Qn::AnyStatus);
    auto nameLess = [](const QnResourcePtr& l, const QnResourcePtr& r) { return l->getName() < r->getName(); };
    std::sort(servers.begin(), servers.end(), nameLess);

    m_model = new QnResourceListModel(this);
    m_model->setHasCheckboxes(false);
    m_model->setUserCheckable(false);
    m_model->setReadOnly(true);
    m_model->setResources(servers);

    ui->serverComboBox->setModel(m_model);
    ui->serverComboBox->setEditable(false);

    /* Set up name edit. */
    QStringList usedNames;
    for (const auto& camera: resourcePool()->getAllCameras())
        usedNames.push_back(camera->getName());

    QString name = nx::utils::generateUniqueString(
        usedNames,
        tr("Wearable Camera"),
        tr("Wearable Camera %1"));

    ui->nameEdit->setText(name);

    setResizeToContentsMode(Qt::Vertical);
}

QnNewWearableCameraDialog::~QnNewWearableCameraDialog()
{
}

QString QnNewWearableCameraDialog::name() const
{
    return ui->nameEdit->text();
}

const QnMediaServerResourcePtr QnNewWearableCameraDialog::server() const
{
    QVariant data = ui->serverComboBox->currentData(Qn::ResourceRole);
    return data.value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
}

