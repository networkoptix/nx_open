#ifndef API_MODULE_DATA_H
#define API_MODULE_DATA_H

#include "api_data.h"
#include <utils/network/module_information.h>

namespace ec2 {

    struct ApiModuleData : ApiData {
        QnModuleInformation moduleInformation;
        bool isAlive;

        ApiModuleData() : isAlive(false) {}
    };

#define ApiModuleData_Fields (moduleInformation)(isAlive)

} // namespace ec2


#endif // API_MODULE_DATA_H
