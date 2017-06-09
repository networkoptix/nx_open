#include "axclient_module.h"

#include <client/client_module.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

QnAxClientModule::QnAxClientModule(QObject *parent) :
    QObject(parent)
{
    QnStartupParameters startupParams;
#ifdef _DEBUG
    startupParams.logLevel = "DEBUG";
#else
    startupParams.logLevel = "INFO";
#endif

    m_clientModule.reset(new QnClientModule(startupParams));
    qnSettings->setLightMode(Qn::LightModeActiveX);
    qnRuntime->setActiveXMode(true);
}

QnAxClientModule::~QnAxClientModule()
{}
