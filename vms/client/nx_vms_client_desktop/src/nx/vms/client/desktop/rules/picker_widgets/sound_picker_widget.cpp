// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

SoundPicker::SoundPicker(WindowContext* context, ParamsWidget* parent):
    PlainFieldPickerWidget<vms::rules::SoundField>(context->system(), parent),
    m_serverNotificationCache{context->workbenchContext()->instance<ServerNotificationCache>()}
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_comboBox = new QComboBox;
    contentLayout->addWidget(m_comboBox);

    m_manageButton = new QPushButton;
    m_manageButton->setText(tr("Manage"));

    contentLayout->addWidget(m_manageButton);

    contentLayout->setStretch(0, 1);

    m_contentWidget->setLayout(contentLayout);

    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    m_comboBox->setModel(soundModel);

    connect(
        m_comboBox,
        &QComboBox::currentIndexChanged,
        this,
        &SoundPicker::onCurrentIndexChanged);

    connect(
        m_manageButton,
        &QPushButton::clicked,
        this,
        &SoundPicker::onManageButtonClicked);
}

void SoundPicker::updateUi()
{
    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    QSignalBlocker blocker{m_comboBox};
    m_comboBox->setCurrentIndex(soundModel->rowByFilename(theField()->value()));
}

void SoundPicker::onCurrentIndexChanged(int index)
{
    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    theField()->setValue(soundModel->filenameByRow(index));
}

void SoundPicker::onManageButtonClicked()
{
    QnNotificationSoundManagerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    m_comboBox->setCurrentIndex(soundModel->rowByFilename(theField()->value()));
}

} // namespace nx::vms::client::desktop::rules
