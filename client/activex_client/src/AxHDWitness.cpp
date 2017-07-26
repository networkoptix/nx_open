#include "AxHDWitness.h"

#include <QAxFactory>

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QXmlStreamWriter>

#include <QtCore/private/qthread_p.h>

#include <axclient/axclient_module.h>
#include <axclient/axclient_window.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <nx/utils/crash_dump/systemexcept.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include <nx/utils/log/log.h>

static QtMessageHandler defaultMsgHandler = 0;

extern HHOOK qax_hhook;

extern LRESULT QT_WIN_CALLBACK axs_FilterProc(int nCode, WPARAM wParam, LPARAM lParam);

LRESULT QT_WIN_CALLBACK qn_FilterProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    QThread *thread = QThread::currentThread();
    if (thread == NULL || QThreadData::get2(thread)->loopLevel == 0)
    {
        return axs_FilterProc(nCode, wParam, lParam);
    }
    else
    {
        return CallNextHookEx(qax_hhook, nCode, wParam, lParam);
    }
}

AxHDWitness::AxHDWitness(QWidget* parent, const char* name)
    : m_isInitialized(false)
    , m_axClientModule(nullptr)
    , m_axClientWindow(nullptr)
{
    Q_UNUSED(parent);
    Q_UNUSED(name);

    initialize();
}

AxHDWitness::~AxHDWitness()
{
    finalize();
}


bool AxHDWitness::event(QEvent *event)
{
    if (event->type() == QEvent::Show)
        qnAxClient->show();

    bool result = QWidget::event(event);

    switch (event->type())
    {
        case QEvent::WindowBlocked:
        {
            HWND hwnd = GetAncestor((HWND)this->winId(), GA_ROOT);
            EnableWindow(hwnd, FALSE);
            break;
        }
        case QEvent::WindowUnblocked:
        {
            HWND hwnd = GetAncestor((HWND)this->winId(), GA_ROOT);
            EnableWindow(hwnd, TRUE);
            break;
        }
        default:
            break;
    }

    return result;
}

void AxHDWitness::initialize()
{
    NX_ASSERT(!m_isInitialized, Q_FUNC_INFO, "Double initialization");
    if (m_isInitialized)
        return;

    doInitialize();

    if (UnhookWindowsHookEx(qax_hhook))
        qax_hhook = SetWindowsHookEx(WH_GETMESSAGE, qn_FilterProc, 0, GetCurrentThreadId());

    m_isInitialized = true;
}

void AxHDWitness::finalize()
{
    NX_ASSERT(m_isInitialized, Q_FUNC_INFO, "Double finalization");
    if (!m_isInitialized)
        return;

    doFinalize();
    m_isInitialized = false;
}

void AxHDWitness::play()
{
    qnAxClient->play();
}

double AxHDWitness::minimalSpeed()
{
    return qnAxClient->minimalSpeed();
}

double AxHDWitness::maximalSpeed()
{
    return qnAxClient->maximalSpeed();
}

double AxHDWitness::speed()
{
    return qnAxClient->speed();
}

void AxHDWitness::setSpeed(double speed)
{
    qnAxClient->setSpeed(speed);
}

qint64 AxHDWitness::currentTime()
{
    return qnAxClient->currentTimeUsec();
}

void AxHDWitness::setCurrentTime(qint64 time)
{
    qnAxClient->setCurrentTimeUsec(time / 10000); // CY scaling...
}

void AxHDWitness::jumpToLive()
{
    qnAxClient->jumpToLive();
}

void AxHDWitness::pause()
{
    qnAxClient->pause();
}

void AxHDWitness::nextFrame()
{
    qnAxClient->nextFrame();
}

void AxHDWitness::prevFrame()
{
    qnAxClient->prevFrame();
}

void AxHDWitness::clear()
{
    qnAxClient->clear();
}

void AxHDWitness::addResourceToLayout(const QString &id, const QString &timestamp)
{
    addResourcesToLayout(id, timestamp);
}

void AxHDWitness::addResourcesToLayout(const QString &ids, const QString &timestamp)
{
    QList<QnUuid> uuids;
    for (const QString& id : ids.split(L'|'))
    {
        QnUuid uuid = QnUuid::fromStringSafe(id);
        if (!uuid.isNull())
            uuids << uuid;
    }
    qnAxClient->addResourcesToLayout(uuids, timestamp.toLongLong());
}

void AxHDWitness::removeFromCurrentLayout(const QString &uniqueId)
{
    qnAxClient->removeFromCurrentLayout(QnUuid::fromHardwareId(uniqueId));
}

QString AxHDWitness::resourceListXml()
{
    QString result;

    QXmlStreamWriter writer(&result);
    writer.writeStartDocument();
    writer.writeStartElement(lit("resources"));

    const auto resPool = qnClientCoreModule->commonModule()->resourcePool();

    for (const QnVirtualCameraResourcePtr &camera: resPool->getAllCameras(QnResourcePtr(), true))
    {
        writer.writeStartElement(lit("resource"));
        writer.writeAttribute(lit("uniqueId"), camera->getUniqueId());
        writer.writeAttribute(lit("isCamera"), lit("1"));
        writer.writeAttribute(lit("name"), camera->getName());
        writer.writeAttribute(lit("ipAddress"), camera->getHostAddress());
        writer.writeEndElement();
    }

    writer.writeEndElement();

    return result;
}

void AxHDWitness::reconnect(const QString &url)
{
    qnAxClient->reconnect(url);
}

void AxHDWitness::maximizeItem(const QString &uniqueId)
{
    qnAxClient->maximizeItem(uniqueId);
}

void AxHDWitness::unmaximizeItem(const QString &uniqueId)
{
    qnAxClient->unmaximizeItem(uniqueId);
}

void AxHDWitness::slidePanelsOut()
{
    qnAxClient->slidePanelsOut();
}

bool AxHDWitness::doInitialize()
{
    /* Methods that should be called once and are not reversible. */
    win32_exception::installGlobalUnhandledExceptionHandler();
    AllowSetForegroundWindow(ASFW_ANY);

    /* These attributes must be set before application instance is created. */
    // TODO: #ynikitenkov how is it supposed to work here?
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths(pluginDirs);

    m_axClientModule.reset(new QnAxClientModule());
    m_axClientWindow.reset(new QnAxClientWindow(this));
    connect(qnAxClient, &QnAxClientWindow::connected, this, [this] { emit connectionProcessed(0, QString()); }, Qt::QueuedConnection);
    connect(qnAxClient, &QnAxClientWindow::disconnected, this, [this] { emit connectionProcessed(1, lit("error")); }, Qt::QueuedConnection);

#ifdef QN_DEBUG_REGEXP_STATIC_FIASCO
    static int initializationCount = 0;
    ++initializationCount;
    qDebug() << "init count" << initializationCount;
    if (initializationCount == 2)
    {
        QThread::msleep(10 * 1000);
        QRegExp test(QLatin1String("test"));
        qDebug() << test.exactMatch(QLatin1String("test"));
    }
#endif //QN_DEBUG_REGEXP_STATIC_FIASCO

    QApplication::processEvents();

    return true;
}



void AxHDWitness::doFinalize()
{
    m_axClientWindow.reset(nullptr);
    m_axClientModule.reset(nullptr);

    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();
}
