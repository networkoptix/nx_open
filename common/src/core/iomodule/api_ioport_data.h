#ifndef __IOMODULE_SETTINGS_H
#define __IOMODULE_SETTINGS_H

#include "api_globals.h"
#include "api_data.h"

namespace Qn
{
    struct ApiIOPortSettings: ApiData 
    {
        ApiIOPortSettings(): ApiData(), portType(Qn::PT_Disabled), defaultState(IO_OpenCircut), autoResetTimeoutMs(0) {}

        QString id;
        Qn::IOPortType portType;
        QString inputName;
        QString outputName;
        Qn::IODefaultState defaultState;   // for output only
        int autoResetTimeoutMs; // for output only. Keep output state on during timeout if non zero
    };
    typedef std::vector<ApiIOPortSettings> ApiIOPortSettingsList;
    
#define ApiIOPortSettings_Fields (id)(portType)(inputName)(outputName)(defaultState)(autoResetTimeoutMs)

    struct ApiIOPortState: ApiData 
    {
        ApiIOPortState(): ApiData(), isActive(false) {}

        QString id;
        bool isActive;
    };
    typedef std::vector<ApiIOPortState> ApiIOPortStateList;

#define ApiIOModuleSettingsData_Fields (id)(active)


}

#endif  //__IOMODULE_SETTINGS_H
