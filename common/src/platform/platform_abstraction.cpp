#include "platform_abstraction.h"

#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

QnPlatformAbstraction *QnPlatformAbstraction::s_instance = NULL;

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    QObject(parent) 
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_monitor = new QnGlobalMonitor(QnPlatformMonitor::newInstance(this), this);
    m_notifier = QnPlatformNotifier::newInstance(this);
    m_process = QnPlatformProcess::newInstance(NULL, this);

    if(s_instance) {
        qnWarning("QnPlatformAbstraction instance already exists.");
    } else {
        s_instance = this;
    }
}

QnPlatformAbstraction::~QnPlatformAbstraction() {
    if(s_instance == this)
        s_instance = NULL;
}

QnPlatformProcess *QnPlatformAbstraction::process(QProcess *source) const {
    if(source == NULL)
        return m_process;

    static const char *qn_platformProcessPropertyName = "_qn_platformProcess";
    QnPlatformProcess *result = source->property(qn_platformProcessPropertyName).value<QnPlatformProcess *>();
    if(!result) {
        result = QnPlatformProcess::newInstance(source, source);
        result->setProperty(qn_platformProcessPropertyName, QVariant::fromValue<QnPlatformProcess *>(result));
    }

    return result;
}
