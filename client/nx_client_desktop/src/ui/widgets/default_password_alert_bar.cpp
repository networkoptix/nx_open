#include "default_password_alert_bar.h"

#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

QnDefaultPasswordAlertBar::QnDefaultPasswordAlertBar(QWidget* parent):
    QnAlertBar(parent),
    base_type(parent),
    m_setPasswordButton(new QPushButton(this))
{
    m_setPasswordButton->setFlat(true);
    m_setPasswordButton->setIcon(qnSkin->icon(lit("buttons/checkmark.png")));
    getOverlayLayout()->addWidget(m_setPasswordButton, 0, Qt::AlignRight);

    connect(this, &QnDefaultPasswordAlertBar::targetCamerasChanged,
        this, &QnDefaultPasswordAlertBar::updateState);
    connect(m_setPasswordButton, &QPushButton::clicked, this,
        [this]() { emit changeDefaultPasswordRequest(m_multipleSourceCameras); });

    setCameras(QnVirtualCameraResourceList());
}

void QnDefaultPasswordAlertBar::setCameras(const QnVirtualCameraResourceList& cameras)
{
    const auto target = cameras.filtered(
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->needsToChangeDefaultPassword();
        });

    bool update = setMultipleSourceCameras(cameras.size() > 1);
    update |= setTargetCameras(target);
    if (update)
        updateState();
}

QnVirtualCameraResourceList QnDefaultPasswordAlertBar::targetCameras() const
{
    return m_targetCameras;
}

bool QnDefaultPasswordAlertBar::setMultipleSourceCameras(bool value)
{
    if (m_multipleSourceCameras == value)
        return false;

    m_multipleSourceCameras = value;
    return true;
}

bool QnDefaultPasswordAlertBar::setTargetCameras(const QnVirtualCameraResourceList& cameras)
{
    if (m_targetCameras == cameras)
        return false;

    m_targetCameras = cameras;
    emit targetCamerasChanged();
    return true;
}

void QnDefaultPasswordAlertBar::updateState()
{
    static const auto kSingleCameraAlertText = tr(
        "This camera requires password to be set up.");
    static const auto kMultipleCameraAlertText = tr(
        "Some of selected cameras requires password to be set up.");
    static const auto kAskAdministratorText =
        tr(" Ask your system administrator to do it.");

    const bool hasAdminAccess = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    const auto suffix = hasAdminAccess ? QString() : kAskAdministratorText;
    if (m_targetCameras.isEmpty())
        setText(QString());
    else if (m_multipleSourceCameras)
        setText(kMultipleCameraAlertText + suffix);
    else
        setText(kSingleCameraAlertText + suffix);

    m_setPasswordButton->setText(tr("Set Password", nullptr, m_targetCameras.size()));
    m_setPasswordButton->setVisible(hasAdminAccess);
}
