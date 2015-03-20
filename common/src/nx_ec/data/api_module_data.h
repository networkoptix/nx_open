#ifndef API_MODULE_DATA_H
#define API_MODULE_DATA_H

#include "api_data.h"
#include <utils/network/module_information.h>

namespace ec2 {

    struct ApiModuleData : ApiData {
        QnModuleInformationWithAddresses moduleInformation;
        bool isAlive;

        ApiModuleData() : isAlive(false) {}

        ApiModuleData(const QnModuleInformation &moduleInformation, bool alive) :
            moduleInformation(moduleInformation),
            isAlive(alive)
        {}
    };

#define ApiModuleData_Fields (moduleInformation)(isAlive)

} // namespace ec2


#endif // API_MODULE_DATA_H
