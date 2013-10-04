#include "platform_abstraction.h"

#include <QtWidgets/QApplication>

#include <utils/common/warnings.h>

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    base_type(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_images = QnPlatformImages::newInstance(this);
}

QnPlatformAbstraction::~QnPlatformAbstraction() {
    return;
}
