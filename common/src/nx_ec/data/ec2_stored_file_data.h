
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

    static const QString FOLDER_NAME_PARAM_NAME( QLatin1String("folder") );

    inline void parseHttpRequestParams( const QnRequestParamList& params, ApiStoredFilePath* path )
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == lit("folder"))
                *path = params[i].second;
        }
    }
}

QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( ec2::ApiStoredFileData, (path) (data) )

#endif //EC2_STORED_FILE_DATA_H
