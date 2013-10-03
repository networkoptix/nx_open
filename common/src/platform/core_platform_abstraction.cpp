#include "core_platform_abstraction.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

#include <utils/common/warnings.h>

QnCorePlatformAbstraction::QnCorePlatformAbstraction(QObject *parent):
    QObject(parent) 
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnCorePlatformAbstraction.");

    m_monitor = new QnGlobalMonitor(QnPlatformMonitor::newInstance(this), this);
    m_notifier = QnPlatformNotifier::newInstance(this);
    m_process = QnPlatformProcess::newInstance(NULL, this);
}

QnCorePlatformAbstraction::~QnCorePlatformAbstraction() {
    return;
}

QnPlatformProcess *QnCorePlatformAbstraction::process(QProcess *source) const {
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
