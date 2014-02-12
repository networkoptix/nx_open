
#ifndef EC2_STORED_FILE_DATA_H
#define EC2_STORED_FILE_DATA_H

#include "api_data.h"

#include <nx_ec/binary_serialization_helper.h>


namespace ec2
{
    class ApiStoredFileData
    :
        public ApiData
    {
    public:
        QString path;
        QByteArray fileData;
    };

    typedef std::vector<QString> ApiStoredDirContents;

    typedef QString StoredFilePath;

    inline void parseHttpRequestParams( const QnRequestParamList& params, StoredFilePath* paht )
    {
        //TODO/IMPL
    }
}

QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( ec2::ApiStoredFileData, (path)(fileData) )

#endif //EC2_STORED_FILE_DATA_H
