#include "flir_io_executor.h"
#include <QtWebSockets/QWebSocket>


FlirIOExecutor::FlirIOExecutor():
    m_thread(new QThread())
{
    qRegisterMetaType<QWebSocket*>("QWebSocket*");
    m_thread->setObjectName(lit("FlirIOExecutor thread"));
}

FlirIOExecutor::~FlirIOExecutor()
{
    m_thread->exit();
    m_thread->wait();
}

QThread* FlirIOExecutor::getThread() const
{
    return m_thread;
}
