#include "axclient_module.h"

#include <client/client_module.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

QnAxClientModule::QnAxClientModule(QObject *parent) :
    QObject(parent),
    m_clientModule(new QnClientModule())
{
    qnSettings->setLightMode(Qn::LightModeActiveX);
    qnRuntime->setActiveXMode(true);
}

QnAxClientModule::~QnAxClientModule()
{}
