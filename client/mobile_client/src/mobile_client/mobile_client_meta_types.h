#pragma once

#include <common/common_meta_types.h>

enum class LiteModeType
{
    LiteModeAuto = -1,
    LiteModeDisabled,
    LiteModeEnabled
};

class QnMobileClientMetaTypes {
public:
    static void initialize();

private:
    static void registerMetaTypes();
    static void registerQmlTypes();
};
