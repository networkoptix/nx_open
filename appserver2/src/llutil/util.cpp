#include "util.h"

void LLUtil::changeGuidByteOrder(std::string& guid)
{
    std::string guid_fixed(guid);

    int replace[] = {6, 7, 4, 5, 2, 3, 0, 1, 8, 11, 12, 9, 10, 13, 16, 17, 14, 15};
    
    for (size_t i = 0; i < sizeof(replace) / sizeof(replace[0]); i++) {
        guid_fixed[i] = guid[replace[i]];
    }

    guid = guid_fixed;
}

