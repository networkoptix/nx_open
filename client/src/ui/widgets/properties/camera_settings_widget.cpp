#include "camera_settings_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_criterion.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "multiple_camera_settings_widget.h"
#include "single_camera_settings_widget.h"

QnCameraSettingsWidget::QnCameraSettingsWidget(QWidget *parent, QnWorkbenchContext *context): 
    QWidget(parent),
    QnWorkbenchContextAware(parent, context),
    m_emptyTab(Qn::GeneralSettingsTab)
{
    /* Create per-mode widgets. */
    QLabel *invalidWidget = new QLabel(tr("Cannot edit properties for items of different types."), this);
    invalidWidget->setAlignment(Qt::AlignCenter);
    m_invalidWidget = invalidWidget;

    QLabel *emptyWidget = new QLabel(tr("No cameras selected."), this);
    emptyWidget->setAlignment(Qt::AlignCenter);
    m_emptyWidget = emptyWidget;

    m_multiWidget = new QnMultipleCameraSettingsWidget(this);

    m_singleWidget = new QnSingleCameraSettingsWidget(this);

    connect(m_multiWidget, SIGNAL(moreLicensesRequested()), this, SLOT(at_moreLicensesRequested()));
    connect(m_singleWidget, SIGNAL(moreLicensesRequested()), this, SLOT(at_moreLicensesRequested()));
    connect(m_singleWidget, SIGNAL(advancedSettingChanged()), this, SLOT(at_advancedSettingChanged()));
    connect(m_singleWidget, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(m_multiWidget,  SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));

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

QnCameraSettingsWidget::~QnCameraSettingsWidget() {
    return;
}

QnCameraSettingsWidget::Mode QnCameraSettingsWidget::mode() const {
    return static_cast<Mode>(m_stackedWidget->currentIndex());
}

QnVirtualCameraResourceList QnCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if (m_cameras == cameras)
        return;

    m_cameras = cameras;

    switch(m_cameras.size()) {
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

Qn::CameraSettingsTab QnCameraSettingsWidget::currentTab() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->currentTab();
    case MultiMode: return m_multiWidget->currentTab();
    default: return m_emptyTab;
    }
}

void QnCameraSettingsWidget::setCurrentTab(Mode mode, Qn::CameraSettingsTab tab) {
    switch(mode) {
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

void QnCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    setCurrentTab(mode(), tab);
}

void QnCameraSettingsWidget::setScheduleEnabled(bool enabled) {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->setScheduleEnabled(enabled);
        break;
    case MultiMode:
        m_multiWidget->setScheduleEnabled(enabled);
        break;
    default:
        break;
    }
}

bool QnCameraSettingsWidget::isScheduleEnabled() const {
    switch(mode()) {
    case SingleMode:
        return m_singleWidget->isScheduleEnabled();
    case MultiMode:
        return m_multiWidget->isScheduleEnabled();
    default:
        break;
    }
    return false;
}

bool QnCameraSettingsWidget::hasDbChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasDbChanges();
    case MultiMode: return m_multiWidget->hasDbChanges();
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasCameraChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasCameraChanges();
    case MultiMode: return false;
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasAnyCameraChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasAnyCameraChanges();
    case MultiMode: return false;
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasScheduleControlsChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasScheduleControlsChanges();
    case MultiMode: return m_multiWidget->hasScheduleControlsChanges();
    default: return false;
    }
}

void QnCameraSettingsWidget::clearScheduleControlsChanges() {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->clearScheduleControlsChanges();
        break;
    case MultiMode:
        m_multiWidget->clearScheduleControlsChanges();
        break;
    default:
        break;
    }
}

bool QnCameraSettingsWidget::hasMotionControlsChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasMotionControlsChanges();
    case MultiMode: return false;
    default: return false;
    }
}

void QnCameraSettingsWidget::clearMotionControlsChanges() {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->clearMotionControlsChanges();
        break;
    default:
        break;
    }
}

const QList< QPair< QString, QVariant> >& QnCameraSettingsWidget::getModifiedAdvancedParams() const
{
    switch(mode()) {
    case SingleMode: return m_singleWidget->getModifiedAdvancedParams();
    case MultiMode: return m_multiWidget->getModifiedAdvancedParams();
    default: Q_ASSERT(false); return *(new QList< QPair< QString, QVariant> >());
    }
}

QnMediaServerConnectionPtr QnCameraSettingsWidget::getServerConnection() const
{
    switch(mode()) {
    case SingleMode: return m_singleWidget->getServerConnection();
    case MultiMode: return m_multiWidget->getServerConnection();
    default: return QnMediaServerConnectionPtr(0);
    }
}

bool QnCameraSettingsWidget::isReadOnly() const {
    return m_singleWidget->isReadOnly();
}

void QnCameraSettingsWidget::setReadOnly(bool readOnly) const {
    m_singleWidget->setReadOnly(readOnly);
    m_multiWidget->setReadOnly(readOnly);
}

bool QnCameraSettingsWidget::licensedParametersModified() const
{
    switch( mode() )
    {
        case SingleMode:
            return m_singleWidget->licensedParametersModified();
        case MultiMode:
            return m_multiWidget->licensedParametersModified();
        default:
            return true;
    }
}

void QnCameraSettingsWidget::updateFromResources() {
    switch(mode()) {
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
    switch(mode()) {
    case SingleMode:
        m_singleWidget->reject();
        break;
    case MultiMode:
        m_multiWidget->reject();
        break;
    default:
        break;
    }
}

void QnCameraSettingsWidget::submitToResources() {
    switch(mode()) {
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

void QnCameraSettingsWidget::setMode(Mode mode) {
    Mode oldMode = this->mode();
    if(mode == oldMode)
        return;

    bool oldHasChanges = hasDbChanges() || hasCameraChanges();
    Qn::CameraSettingsTab oldTab = currentTab();

    if(m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        disconnect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    setCurrentTab(mode, oldTab);

    switch(oldMode) {
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

    if(m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        connect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    bool newHasChanges = hasDbChanges() || hasCameraChanges();
    if(oldHasChanges != newHasChanges)
        emit hasChangesChanged();

    emit modeChanged();
}

void QnCameraSettingsWidget::at_moreLicensesRequested() {
    menu()->trigger(Qn::PreferencesLicensesTabAction);
}

void QnCameraSettingsWidget::at_advancedSettingChanged() {
    emit advancedSettingChanged();
}

bool QnCameraSettingsWidget::isValidMotionRegion(){
    if (mode() == SingleMode)
        return m_singleWidget->isValidMotionRegion();
    return true;
}

bool QnCameraSettingsWidget::isValidSecondStream() {
    switch(mode()) {
    case SingleMode:
        return m_singleWidget->isValidSecondStream();
    case MultiMode:
        return m_multiWidget->isValidSecondStream();
    default:
        return true;
    }
}

void QnCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled) {
    switch(mode()) {
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
