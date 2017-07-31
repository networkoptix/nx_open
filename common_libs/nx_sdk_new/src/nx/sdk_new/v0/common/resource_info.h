#pragma once

namespace nx {
namespace sdk {
namespace v0 {

const int kMaxStringParameterLength = 256;
const int kMaxTextParameterLength = 1024;

struct ResourceInfo
{

public:
    ResourceInfo()
    {
        vendor[0] = 0;
        model[0] = 0;
        firmware[0] = 0;
        uid[0] = 0;
        url[0] = 0;
        login[0] = 0;
        password[0] = 0;
    }

public:
    char vendor[kMaxStringParameterLength];
    char model[kMaxStringParameterLength];
    char firmware[kMaxStringParameterLength];
    char uid[kMaxStringParameterLength];
    char url[kMaxTextParameterLength];
    char login[kMaxStringParameterLength];
    char password[kMaxStringParameterLength];
};

} // namespace v0
} // namespace sdk
} // namespace nx
