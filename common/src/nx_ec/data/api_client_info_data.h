#ifndef EC2__CLIENT_INFO_DATA_H
#define EC2__CLIENT_INFO_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

    struct ApiClientInfoData
        : ApiData
    {
        QnUuid id;

		QString cpuArchitecture, cpuModelName;
		qint64 phisicalMemory;
        QString openGLVersion, openGLVendor, openGLRenderer;

		bool operator == (const ApiClientInfoData& rhs) const;
    };
#define ApiClientInfoData_Fields (id) \
    (cpuArchitecture)(cpuModelName)(phisicalMemory) \
    (openGLVersion)(openGLVendor)(openGLRenderer)

} // namespace ec2

#endif // EC2__CLIENT_INFO_DATA_H