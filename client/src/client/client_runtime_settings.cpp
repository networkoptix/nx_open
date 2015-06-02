#include "client_runtime_settings.h"

QnClientRuntimeSettings::QnClientRuntimeSettings(QObject *parent):
    base_type(parent)
{
    init();
    setThreadSafe(true);
}

QnClientRuntimeSettings::~QnClientRuntimeSettings()
{

}
