#include "api_client_info_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiClientInfoData), \
        (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))

	bool ApiClientInfoData::operator == (const ApiClientInfoData& rhs) const
	{
        return id == rhs.id &&
            skin == rhs.skin &&
            systemInfo == rhs.systemInfo &&
            cpuArchitecture == rhs.cpuArchitecture &&
            cpuModelName == rhs.cpuModelName &&
            phisicalMemory == rhs.phisicalMemory &&
            openGLVersion == rhs.openGLVersion &&
            openGLVendor == rhs.openGLVendor &&
            openGLRenderer == rhs.openGLRenderer;
	}

} // namespace ec2
