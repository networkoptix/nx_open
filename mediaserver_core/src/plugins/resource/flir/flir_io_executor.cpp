#include <QtWebSockets/QWebSocket>

#include "flir_io_executor.h"

namespace nx {
namespace plugins {
namespace flir {

IoExecutor::IoExecutor():
    m_thread(new QThread())
{
    qRegisterMetaType<QWebSocket*>("QWebSocket*");
    m_thread->setObjectName(lit("FlirIoExecutor thread"));
}

IoExecutor::~IoExecutor()
{
    m_thread->exit();
    m_thread->wait();
}

QThread* IoExecutor::getThread() const
{
    return m_thread.get();
}

} // namespace flir
} // namespace plugins
} // namespace nx