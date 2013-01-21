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
    m_images = QnPlatformImages::newInstance(this);

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

