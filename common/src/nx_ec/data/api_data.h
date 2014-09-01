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
    ApiFillerData(): seqTo(0) {}
    int seqTo;
};
#define ApiFillerData_Fields (seqTo)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpData);

#endif // QN_API_DATA_H
