#include "flir_io_executor.h"
#include <QtWebSockets/QWebSocket>


FlirIoExecutor::FlirIoExecutor():
    m_thread(new QThread())
{
    qRegisterMetaType<QWebSocket*>("QWebSocket*");
    m_thread->setObjectName(lit("FlirIoExecutor thread"));
}

FlirIoExecutor::~FlirIoExecutor()
{
    m_thread->exit();
    m_thread->wait();
}

QThread* FlirIoExecutor::getThread() const
{
    return m_thread.get();
}
