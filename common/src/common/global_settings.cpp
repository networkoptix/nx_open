/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#include "global_settings.h"

#include <QtGlobal>


namespace Qn
{
    GlobalSettings::GlobalSettings()
    :
        m_httpPort( 0 )
    {
    }

    void GlobalSettings::setHttpPort( int httpPortVal )
    {
        m_httpPort = httpPortVal;
    }

    int GlobalSettings::httpPort() const
    {
        return m_httpPort;
    }

    Q_GLOBAL_STATIC(GlobalSettings, GlobalSettings_instance)

    GlobalSettings* GlobalSettings::instance()
    {
        return GlobalSettings_instance();
    }
}
