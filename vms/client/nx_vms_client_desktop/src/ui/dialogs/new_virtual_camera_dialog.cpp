// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "new_virtual_camera_dialog.h"
#include "ui_new_virtual_camera_dialog.h"

#include <algorithm>

#include <QtWidgets/QPushButton>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/models/resource/resource_list_model.h>

using namespace nx::vms::client::desktop;

QnNewVirtualCameraDialog::QnNewVirtualCameraDialog(
    QWidget* parent, const QnMediaServerResourcePtr& selectedServer)
    :
    base_type(parent),
    ui(new Ui::NewVirtualCameraDialog)
{
    ui->setupUi(this);
    ui->headerLabel->setText(tr(
        "\"Virtual Camera\" is a virtual storage for video files, which could be uploaded to any"
        " server on your System and be accessed by any user."));

    // TODO: #virtualCamera help topics?
    //setHelpTopic(, Qn::);

    /* Set up combobox. */
    QnResourceList servers = systemContext()->resourcePool()->servers();
    auto nameLess =
        [](const QnResourcePtr& l, const QnResourcePtr& r) { return l->getName() < r->getName(); };
    std::sort(servers.begin(), servers.end(), nameLess);
    auto selectedServerIndex = servers.indexOf(selectedServer);
    if (selectedServerIndex == -1)
        selectedServerIndex = servers.indexOf(systemContext()->currentServer());

    m_model = new QnResourceListModel(this);
    m_model->setHasCheckboxes(false);
    m_model->setReadOnly(true);
    m_model->setOptions(QnResourceListModel::AlwaysSelectedOption);
    m_model->setResources(servers);

    ui->serverComboBox->setModel(m_model);
    ui->serverComboBox->setCurrentIndex(selectedServerIndex);
    ui->serverComboBox->setEditable(false);

    if (servers.size() == 1)
    {
        ui->serverComboBox->hide();
        ui->serverLabel->hide();
    }

    /* Set up name edit. */
    QStringList usedNames;
    for (const auto& camera: systemContext()->resourcePool()->getAllCameras())
        usedNames.push_back(camera->getName());

    QString name = nx::utils::generateUniqueString(
        usedNames,
        tr("Virtual Camera"),
        tr("Virtual Camera %1"));

    ui->nameField->setTitle(tr("Name"));
    ui->nameField->setText(name);
    ui->nameField->setValidator(defaultNonEmptyValidator(tr("Name cannot be empty")));

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidget(ui->serverLabel);
    aligner->addWidget(ui->nameField);

    setResizeToContentsMode(Qt::Vertical);
}

QnNewVirtualCameraDialog::~QnNewVirtualCameraDialog()
{
}

QString QnNewVirtualCameraDialog::name() const
{
    return ui->nameField->text();
}

const QnMediaServerResourcePtr QnNewVirtualCameraDialog::server() const
{
    QVariant data = ui->serverComboBox->currentData(Qn::ResourceRole);
    return data.value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
}

void QnNewVirtualCameraDialog::accept()
{
    if (!ui->nameField->isValid()) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus(Qt::TabFocusReason);
        return;
    }

    base_type::accept();
}
