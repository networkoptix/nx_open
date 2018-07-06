#ifndef __API_MISC_DATA_H_
#define __API_MISC_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include <nx/utils/uuid.h>


namespace ec2
{
    struct ApiMiscData: nx::vms::api::Data
    {
        ApiMiscData() = default;
        ApiMiscData(const QByteArray &name, const QByteArray &value)
            : name(name),
              value(value)
        {}

        QByteArray name;
        QByteArray value;
    };

#define ApiMiscData_Fields (name)(value)

}

#endif // __API_MISC_DATA_H_
