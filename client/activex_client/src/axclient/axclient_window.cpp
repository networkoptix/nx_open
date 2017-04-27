#include "axclient_window.h"

#include <client/client_message_processor.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>

#ifdef _DEBUG
    #define QN_WAIT_FOR_DEBUGGER
#endif


namespace {
    /* After opening layout, set timeline window to 5 minutes. */
    const qint64 displayWindowLengthMs = 5 * 60 * 1000;

}

QnAxClientWindow::QnAxClientWindow(QWidget *parent)
    : base_type(parent)
    , m_parentWidget(parent)
    , m_context(nullptr)
    , m_mainWindow(nullptr)
{
}

QnAxClientWindow::~QnAxClientWindow() {
    /* Main Window must be destroyed before the context. */
    if (m_mainWindow)
        m_mainWindow.reset(nullptr);
    if (m_context)
        m_context->menu()->trigger(QnActions::BeforeExitAction);
}

void QnAxClientWindow::show() {
    NX_ASSERT(m_parentWidget, Q_FUNC_INFO, "Parent widget must be set");

    if(!m_mainWindow) {
        createMainWindow();

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(m_mainWindow.data());
        m_parentWidget->setLayout(mainLayout);
    }

    executeDelayed([this] {
 //       m_context->action(QnActions::FullscreenAction)->setChecked(true);
/*        m_mainWindow->showFullScreen(); */
    });
}

void QnAxClientWindow::jumpToLive() {
    if (m_context)
        m_context->navigator()->setLive(true);
}

void QnAxClientWindow::play() {
    if(m_context)
        m_context->navigator()->setPlaying(true);
}

void QnAxClientWindow::pause() {
    if (m_context)
        m_context->navigator()->setPlaying(false);
}

void QnAxClientWindow::nextFrame() {
    if (m_context)
        m_context->navigator()->stepForward();
}

void QnAxClientWindow::prevFrame() {
    if (m_context)
        m_context->navigator()->stepBackward();
}

void QnAxClientWindow::clear() {
    if (m_context)
        m_context->workbench()->currentLayout()->clear();
}

double QnAxClientWindow::minimalSpeed() const {
    return m_context ? m_context->navigator()->minimalSpeed() : 0.0;
}

double QnAxClientWindow::maximalSpeed() const {
    return m_context ? m_context->navigator()->maximalSpeed() : 0.0;
}

double QnAxClientWindow::speed() const {
    return m_context ? m_context->navigator()->speed() : 0.0;
}

void QnAxClientWindow::setSpeed(double speed) {
    if (m_context)
        m_context->navigator()->setSpeed(speed);
}

qint64 QnAxClientWindow::currentTimeUsec() const {
    return m_context ? m_context->navigator()->positionUsec() : 0;
}

void QnAxClientWindow::setCurrentTimeUsec(qint64 timeUsec) {
    if (m_context)
        m_context->navigator()->setPosition(timeUsec);
}

void QnAxClientWindow::addResourcesToLayout(const QList<QnUuid> &uniqueIds, qint64 timeStampMs)
{
    NX_LOG("Adding resources to layout", cl_logINFO);
    if (!m_context || !m_context->user())
    {
        NX_LOG("Not logged in, returning...", cl_logINFO);
        return;
    }

    QnResourceList resources;
    for(const auto& id: uniqueIds)
    {
        QnResourcePtr resource = qnResPool->getResourceById(id);
        NX_LOG(lm("Found resource: %1").arg(resource ? resource->getName() : "???"), cl_logDEBUG1);
        if (resource)
            resources << resource;
    }

    if (resources.isEmpty())
    {
        NX_LOG("No cameras found, returning...", cl_logINFO);
        return;
    }

    qint64 maxTime = qnSyncTime->currentMSecsSinceEpoch();
    if (timeStampMs < 0)
        timeStampMs = maxTime; /* Live */

    QnTimePeriod period(timeStampMs - displayWindowLengthMs/2, displayWindowLengthMs);
    if (period.endTimeMs() > maxTime) {
        period.startTimeMs = maxTime - displayWindowLengthMs;
        period.durationMs = displayWindowLengthMs;
    }

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setParentId(m_context->user()->getId());
    layout->setCellSpacing(0);
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState(true, timeStampMs, 1.0)));
    qnResPool->addResource(layout);

    QnWorkbenchLayout *wlayout = new QnWorkbenchLayout(layout, this);
    m_context->workbench()->addLayout(wlayout);
    m_context->workbench()->setCurrentLayout(wlayout);

    m_context->menu()->trigger(QnActions::OpenInCurrentLayoutAction, QnActionParameters(resources).withArgument(Qn::ItemTimeRole, timeStampMs));

    for (QnWorkbenchItem *item: wlayout->items())
        item->setData(Qn::ItemSliderWindowRole, qVariantFromValue(period));

    auto timeSlider = m_context->navigator()->timeSlider();
    /* Disable unused options to make sure our window will be set to fixed size. */
    timeSlider->setOption(QnTimeSlider::StickToMinimum, false);
    timeSlider->setOption(QnTimeSlider::StickToMaximum, false);

    /* Set range to maximum allowed value, so we will not constrain window by any values. */
    timeSlider->setRange(0, maxTime);
    timeSlider->setWindow(period.startTimeMs, period.endTimeMs(), false);
}

void QnAxClientWindow::removeFromCurrentLayout(const QnUuid& uniqueId)
{
    if (!m_context)
        return;

    auto layout = m_context->workbench()->currentLayout()->resource();
    if (layout)
        layout->removeItem(layout->getItem(uniqueId));
}

void QnAxClientWindow::reconnect(const QString &url) {
    if (m_context)
        m_context->menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, url) );
}

void QnAxClientWindow::maximizeItem(const QString &uniqueId) {
    if (!m_context)
        return;

    QSet<QnWorkbenchItem *> items = m_context->workbench()->currentLayout()->items(uniqueId);
    if(items.isEmpty())
        return;
    m_context->menu()->trigger(QnActions::MaximizeItemAction, m_context->display()->widget(*items.begin()));
}

void QnAxClientWindow::unmaximizeItem(const QString &uniqueId) {
    if (!m_context)
        return;

    QSet<QnWorkbenchItem *> items = m_context->workbench()->currentLayout()->items(uniqueId);
    if(items.isEmpty())
        return;
    m_context->menu()->trigger(QnActions::UnmaximizeItemAction, m_context->display()->widget(*items.begin()));
}

void QnAxClientWindow::slidePanelsOut() {
    if (m_context)
        m_context->menu()->trigger(QnActions::MaximizeAction);
}

void QnAxClientWindow::createMainWindow() {
    NX_ASSERT(m_mainWindow == NULL, Q_FUNC_INFO, "Double initialization");

    m_accessController.reset(new QnWorkbenchAccessController());
    m_context.reset(new QnWorkbenchContext(m_accessController.data()));

    //TODO: #GDM is it really needed here?
    QnActions::IDType effectiveMaximizeActionId = QnActions::FullscreenAction;
    m_context->menu()->registerAlias(QnActions::EffectiveMaximizeAction, effectiveMaximizeActionId);

    m_mainWindow.reset(new QnMainWindow(m_context.data()));
    m_mainWindow->setOptions(m_mainWindow->options() & ~(QnMainWindow::TitleBarDraggable));

    m_mainWindow->resize(100, 100);
    m_context->setMainWindow(m_mainWindow.data());

#ifdef _DEBUG
    /* Show FPS in debug. */
    m_context->menu()->trigger(QnActions::ShowFpsAction);
#endif

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived,  this, &QnAxClientWindow::connected);
    connect(m_context->action(QnActions::ExitAction), &QAction::triggered,                         this, &QnAxClientWindow::disconnected);

#ifdef QN_WAIT_FOR_DEBUGGER
    QMessageBox::information(m_mainWindow.data(), "Waiting...", "Waiting for debugger to be attached.");
#endif
}
