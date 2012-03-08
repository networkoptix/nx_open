#include "camera_settings_widget.h"

#include <QtGui/QLabel>
#include <QtGui/QStackedWidget>
#include <QtGui/QVBoxLayout>

#include "multiple_camera_settings_widget.h"
#include "single_camera_settings_widget.h"

QnCameraSettingsWidget::QnCameraSettingsWidget(QWidget *parent): 
    QWidget(parent),
    m_emptyTab(Qn::GeneralSettingsTab)
{
    /* Create per-mode widgets. */
    QLabel *emptyWidget = new QLabel(tr("No cameras selected."), this);
    emptyWidget->setAlignment(Qt::AlignCenter);
    m_emptyWidget = emptyWidget;

    m_multiWidget = new QnMultipleCameraSettingsWidget(this);

    m_singleWidget = new QnSingleCameraSettingsWidget(this);

    /* Stack per-mode widgets. */
    m_stackedWidget = new QStackedWidget(this);
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

const QnVirtualCameraResourceList &QnCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if(m_cameras == cameras)
        return;

    m_cameras = cameras;

    switch(m_cameras.size()) {
    case 0: 
        setMode(EmptyMode);
        break;
    case 1: 
        setMode(SingleMode);
        break;
    default: 
        setMode(MultiMode);
        break;
    }
}

Qn::CameraSettingsTab QnCameraSettingsWidget::currentTab() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->currentTab();
    case MultiMode: return m_multiWidget->currentTab();
    default: return m_emptyTab;
    }
}

void QnCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    switch(mode()) {
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

bool QnCameraSettingsWidget::hasChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasChanges();
    case MultiMode: return m_multiWidget->hasChanges();
    default: return false;
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
    if(mode == this->mode())
        return;

    bool oldHasChanges = hasChanges();
    Qn::CameraSettingsTab oldTab = currentTab();

    if(m_stackedWidget->currentIndex() != EmptyMode)
        disconnect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    switch(m_stackedWidget->currentIndex()) {
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

    switch(m_stackedWidget->currentIndex()) {
    case SingleMode:
        m_singleWidget->setCamera(m_cameras.front());
        break;
    case MultiMode:
        m_multiWidget->setCameras(m_cameras);
        break;
    default:
        break;
    }

    if(m_stackedWidget->currentIndex() != EmptyMode)
        connect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    setCurrentTab(oldTab);

    bool newHasChanges = hasChanges();
    if(oldHasChanges != newHasChanges)
        emit hasChangesChanged();
}

