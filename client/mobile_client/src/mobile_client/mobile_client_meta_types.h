#pragma once

#include <common/common_meta_types.h>

enum class LiteModeType
{
    LiteModeAuto = -1,
    LiteModeDisabled,
    LiteModeEnabled
};

enum class AutoLoginMode
{
    Auto = -1,
    Enabled,
    Disabled
};

class QnMobileClientMetaTypes {
public:
    static void initialize();

private:
    static void registerMetaTypes();
    static void registerQmlTypes();
};
