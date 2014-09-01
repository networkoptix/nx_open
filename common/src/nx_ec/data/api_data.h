#ifndef QN_API_DATA_H
#define QN_API_DATA_H

#include "api_globals.h"

namespace ec2 {
    
    struct ApiData {
        virtual ~ApiData() {}
    };
    #define ApiData_Fields ()

    struct ApiIdData: ApiData {
        QUuid id;
    };
#define ApiIdData_Fields (id)

struct ApiDatabaseDumpData: public ApiData {
    QByteArray data;

};
#define ApiDatabaseDumpData_Fields (data)

struct ApiFillerData: public ApiData 
{
    ApiFillerData(): filler(0) {}
    int filler;
};
#define ApiFillerData_Fields (filler)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpData);

#endif // QN_API_DATA_H
