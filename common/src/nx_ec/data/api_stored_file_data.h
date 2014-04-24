#ifndef EC2_STORED_FILE_DATA_H
#define EC2_STORED_FILE_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiStoredFileData: ApiData
    {
        QString path;
        QByteArray data;
    };

    typedef QString ApiStoredFilePath; // TODO: #Elric struct => extendable?

    typedef std::vector<QString> ApiStoredDirContents;

    //QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( ApiStoredFileData, (path) (data) )
    //QN_FUSION_DECLARE_FUNCTIONS(ApiStoredFileData, (binary))



    /*typedef QString ApiStoredFilePath;

    static const QString FOLDER_NAME_PARAM_NAME( QLatin1String("folder") );

    inline void parseHttpRequestParams( const QnRequestParamList& params, ApiStoredFilePath* path )
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == lit("folder"))
                *path = params[i].second;
        }
    }*/
}


#endif //EC2_STORED_FILE_DATA_H
