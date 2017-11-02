#include "camera_settings_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "multiple_camera_settings_widget.h"
#include "single_camera_settings_widget.h"

QnCameraSettingsWidget::QnCameraSettingsWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    m_emptyTab(Qn::GeneralSettingsTab)
{
    /* Create per-mode widgets. */
    QLabel *invalidWidget = new QLabel(tr("Cannot edit properties for items of different types."), this);
    invalidWidget->setAlignment(Qt::AlignCenter);
    m_invalidWidget = invalidWidget;

    QLabel *emptyWidget = new QLabel(tr("No device selected."), this);
    emptyWidget->setAlignment(Qt::AlignCenter);
    m_emptyWidget = emptyWidget;

    m_multiWidget = new QnMultipleCameraSettingsWidget(this);

    m_singleWidget = new QnSingleCameraSettingsWidget(this);

    /* Stack per-mode widgets. */
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->insertWidget(InvalidMode, m_invalidWidget);
    m_stackedWidget->insertWidget(EmptyMode, m_emptyWidget);
    m_stackedWidget->insertWidget(SingleMode, m_singleWidget);
    m_stackedWidget->insertWidget(MultiMode, m_multiWidget);
    m_stackedWidget->setCurrentIndex(EmptyMode);

    /* Init this widget. */
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
}

QnCameraSettingsWidget::~QnCameraSettingsWidget()
{
    return;
}

QnCameraSettingsWidget::Mode QnCameraSettingsWidget::mode() const
{
    return static_cast<Mode>(m_stackedWidget->currentIndex());
}

QnVirtualCameraResourceList QnCameraSettingsWidget::cameras() const
{
    return m_cameras;
}

void QnCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    m_cameras = cameras;

    switch (m_cameras.size())
    {
        case 0:
            setMode(EmptyMode);
            break;
        case 1:
            m_singleWidget->setCamera(m_cameras.front());
            setMode(SingleMode);
            break;
        default:
            m_multiWidget->setCameras(m_cameras);
            setMode(MultiMode);
            break;
    }

    emit resourcesChanged();
}

Qn::CameraSettingsTab QnCameraSettingsWidget::currentTab() const
{
    switch (mode())
    {
        case SingleMode: return m_singleWidget->currentTab();
        case MultiMode: return m_multiWidget->currentTab();
        default: return m_emptyTab;
    }
}

void QnCameraSettingsWidget::setCurrentTab(Mode mode, Qn::CameraSettingsTab tab)
{
    switch (mode)
    {
        case SingleMode:
            m_singleWidget->setCurrentTab(tab);
            break;
        case MultiMode:
            m_multiWidget->setCurrentTab(tab);
            break;
        default:
            m_emptyTab = tab;
            break;
    }
}

void QnCameraSettingsWidget::setLockedMode(bool value)
{
    switch (mode())
    {
        case SingleMode:
            m_singleWidget->setLockedMode(value);
            break;
        case MultiMode:
            m_multiWidget->setLockedMode(value);
            break;
        default:
            break;
    }

}

void QnCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab)
{
    setCurrentTab(mode(), tab);
}

bool QnCameraSettingsWidget::hasDbChanges() const
{
    switch (mode())
    {
        case SingleMode: return m_singleWidget->hasChanges();
        case MultiMode: return m_multiWidget->hasDbChanges();
        default: return false;
    }
}

bool QnCameraSettingsWidget::isReadOnly() const
{
    return m_singleWidget->isReadOnly();
}

void QnCameraSettingsWidget::setReadOnly(bool readOnly) const
{
    m_singleWidget->setReadOnly(readOnly);
    m_multiWidget->setReadOnly(readOnly);
}

void QnCameraSettingsWidget::updateFromResources()
{
    switch (mode())
    {
        case SingleMode:
            m_singleWidget->updateFromResource();
            break;
        case MultiMode:
            m_multiWidget->updateFromResources();
            break;
        default:
            break;
    }
}

void QnCameraSettingsWidget::reject()
{
    switch (mode())
    {
        case SingleMode:
            m_singleWidget->updateFromResource(true);
            break;
        case MultiMode:
            m_multiWidget->updateFromResources();
            break;
        default:
            break;
    }
}

void QnCameraSettingsWidget::submitToResources()
{
    switch (mode())
    {
        case SingleMode:
            m_singleWidget->submitToResource();
            break;
        case MultiMode:
            m_multiWidget->submitToResources();
            break;
        default:
            break;
    }
}

void QnCameraSettingsWidget::setMode(Mode mode)
{
    Mode oldMode = this->mode();
    if (mode == oldMode)
        return;

    bool oldHasChanges = hasDbChanges();
    Qn::CameraSettingsTab oldTab = currentTab();

    if (m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        disconnect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    setCurrentTab(mode, oldTab);

    switch (oldMode)
    {
        case SingleMode:
            m_singleWidget->setCamera(QnVirtualCameraResourcePtr());
            break;
        case MultiMode:
            m_multiWidget->setCameras(QnVirtualCameraResourceList());
            break;
        default:
            break;
    }

    m_stackedWidget->setCurrentIndex(mode);

    if (m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        connect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    bool newHasChanges = hasDbChanges();
    if (oldHasChanges != newHasChanges)
        emit hasChangesChanged();

    emit modeChanged();
}

bool QnCameraSettingsWidget::isValidMotionRegion()
{
    if (mode() == SingleMode)
        return m_singleWidget->isValidMotionRegion();
    return true;
}

bool QnCameraSettingsWidget::isValidSecondStream()
{
    switch (mode())
    {
        case SingleMode:
            return m_singleWidget->isValidSecondStream();
        case MultiMode:
            return m_multiWidget->isValidSecondStream();
        default:
            return true;
    }
}

void QnCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled)
{
    switch (mode())
    {
        case SingleMode:
            m_singleWidget->setExportScheduleButtonEnabled(enabled);
            break;
        case MultiMode:
            m_multiWidget->setExportScheduleButtonEnabled(enabled);
            break;
        default:
            break;
    }
}
