#ifndef __EC2_BOOKMARK_DATA_H_
#define __EC2_BOOKMARK_DATA_H_

#include "api_data.h"
#include "api_globals.h"

namespace ec2
{
    struct ApiCameraBookmarkTagData: ApiData {
        QString name;

        ApiCameraBookmarkTagData(){};
        ApiCameraBookmarkTagData(const QString &name): name(name) {}
    };
    #define ApiCameraBookmarkTagData_Fields (name)
}

#endif //__EC2_BOOKMARK_DATA_H_
