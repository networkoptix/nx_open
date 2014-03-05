
#ifndef EC2_STORED_FILE_DATA_H
#define EC2_STORED_FILE_DATA_H

#include "api_data.h"

#include <nx_ec/binary_serialization_helper.h>
#include <utils/common/request_param.h>


namespace ec2
{
    class ApiStoredFileData
    :
        public ApiData
    {
    public:
        QString path;
        QByteArray data;
    };

    typedef std::vector<QString> ApiStoredDirContents;

    typedef QString ApiStoredFilePath;

    inline void parseHttpRequestParams( const QnRequestParamList& /*params*/, ApiStoredFilePath* /*path*/ )
    {
        //TODO/IMPL
    }
}

QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( ec2::ApiStoredFileData, (path) (data) )

#endif //EC2_STORED_FILE_DATA_H
