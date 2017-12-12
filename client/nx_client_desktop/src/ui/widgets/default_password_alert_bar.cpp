#include "default_password_alert_bar.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/style/helper.h>
#include <ui/workbench/workbench_access_controller.h>

QnDefaultPasswordAlertBar::QnDefaultPasswordAlertBar(QWidget* parent):
    QnAlertBar(parent),
    base_type(parent),
    m_setPasswordButton(new QPushButton(this))
{
    m_setPasswordButton->setFlat(true);
    m_setPasswordButton->setIcon(qnSkin->icon(lit("buttons/password.png")));

    getOverlayLayout()->setContentsMargins(0, 0, style::Metrics::kDefaultTopLevelMargin, 0);
    getOverlayLayout()->addWidget(m_setPasswordButton, 0, Qt::AlignRight);

    connect(this, &QnDefaultPasswordAlertBar::targetCamerasChanged,
        this, &QnDefaultPasswordAlertBar::updateState);
    connect(m_setPasswordButton, &QPushButton::clicked, this,
        [this]() { emit changeDefaultPasswordRequest(); });

    updateState();
}

QnDefaultPasswordAlertBar::~QnDefaultPasswordAlertBar()
{
}

QnVirtualCameraResourceSet QnDefaultPasswordAlertBar::cameras() const
{
    return m_cameras;
}

void QnDefaultPasswordAlertBar::setCameras(const QnVirtualCameraResourceSet& cameras)
{
    if (m_cameras == cameras)
        return;

    m_cameras = cameras;

    updateState();
    emit targetCamerasChanged();
}

bool QnDefaultPasswordAlertBar::useMultipleForm() const
{
    return m_useMultipleForm;
}

void QnDefaultPasswordAlertBar::setUseMultipleForm(bool value)
{
    if (m_useMultipleForm == value)
        return;

    m_useMultipleForm = value;
    updateState();
}

void QnDefaultPasswordAlertBar::updateState()
{
    static const auto kSingleCameraAlertText = tr(
        "This camera requires password to be set up.");
    static const auto kMultipleCameraAlertText = tr(
        "Some of selected cameras requires password to be set up.");
    static const auto kAskAdministratorText = L' ' +
        tr("Ask your system administrator to do it.");

    const bool hasAdminAccess = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    const auto suffix = hasAdminAccess ? QString() : kAskAdministratorText;
    if (m_cameras.empty())
        setText(QString());
    else if (m_useMultipleForm)
        setText(kMultipleCameraAlertText + suffix);
    else
        setText(kSingleCameraAlertText + suffix);

    m_setPasswordButton->setText(tr("Set Password", "", m_cameras.size()));
    m_setPasswordButton->setVisible(hasAdminAccess);
}
