#include "client_platform_abstraction.h"

#include <utils/common/warnings.h>

QnClientPlatformAbstraction *QnClientPlatformAbstraction::s_instance = NULL;

QnClientPlatformAbstraction::QnClientPlatformAbstraction(QObject *parent) :
    QObject(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnClientPlatformAbstraction.");

    m_images = QnPlatformImages::newInstance(this);

    if(s_instance) {
        qnWarning("QnClientPlatformAbstraction instance already exists.");
    } else {
        s_instance = this;
    }
}

QnClientPlatformAbstraction::~QnClientPlatformAbstraction() {
    if(s_instance == this)
        s_instance = NULL;
}
