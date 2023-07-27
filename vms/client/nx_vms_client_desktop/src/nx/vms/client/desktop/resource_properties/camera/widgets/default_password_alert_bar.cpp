// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "default_password_alert_bar.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}}},
};

} // namespace

namespace nx::vms::client::desktop {

DefaultPasswordAlertBar::DefaultPasswordAlertBar(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_setPasswordButton(new QPushButton(this))
{
    m_setPasswordButton->setFlat(true);
    m_setPasswordButton->setIcon(qnSkin->icon("text_buttons/password_20.svg", kIconSubstitutions));

    overlayLayout()->setContentsMargins(0, 0, style::Metrics::kDefaultTopLevelMargin, 0);
    overlayLayout()->addWidget(m_setPasswordButton, 0, Qt::AlignRight);

    connect(this, &DefaultPasswordAlertBar::targetCamerasChanged,
        this, &DefaultPasswordAlertBar::updateState);
    connect(m_setPasswordButton, &QPushButton::clicked, this,
        [this]() { emit changeDefaultPasswordRequest(); });

    updateState();
}

DefaultPasswordAlertBar::~DefaultPasswordAlertBar()
{
}

QnVirtualCameraResourceSet DefaultPasswordAlertBar::cameras() const
{
    return m_cameras;
}

void DefaultPasswordAlertBar::setCameras(const QnVirtualCameraResourceSet& cameras)
{
    if (m_cameras == cameras)
        return;

    m_cameras = cameras;

    updateState();
    emit targetCamerasChanged();
}

bool DefaultPasswordAlertBar::useMultipleForm() const
{
    return m_useMultipleForm;
}

void DefaultPasswordAlertBar::setUseMultipleForm(bool value)
{
    if (m_useMultipleForm == value)
        return;

    m_useMultipleForm = value;
    updateState();
}

void DefaultPasswordAlertBar::updateState()
{
    static const auto kSingleCameraAlertText = tr(
        "This camera requires password to be set up.");
    static const auto kMultipleCameraAlertText = tr(
        "Some of selected cameras require password to be set up.");
    static const auto kAskAdministratorText = ' ' +
        tr("Ask your system administrator to do it.");

    const bool hasAdminAccess = systemContext()->accessController()->hasGlobalPermissions(
        GlobalPermission::powerUser);

    const auto suffix = hasAdminAccess ? QString() : kAskAdministratorText;
    if (m_cameras.empty())
        setText(QString());
    else if (m_useMultipleForm)
        setText(kMultipleCameraAlertText + suffix);
    else
        setText(kSingleCameraAlertText + suffix);

    m_setPasswordButton->setText(tr("Set Password"));
    m_setPasswordButton->setVisible(hasAdminAccess);
}

} // namespace nx::vms::client::desktop
