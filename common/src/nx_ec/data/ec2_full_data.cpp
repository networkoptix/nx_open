#include "ec2_full_data.h"
#include "core/resource/media_server_resource.h"


static const int KV_RESOURCE_PARAMS_COMMON_COUNT = 10;

namespace ec2
{
    void ApiFullData::toResourceList(QnFullResourceData& outData, const ResourceContext& ctx) const
    {
        resTypes.toResourceTypeList(outData.resTypes);

        servers.toResourceList(outData.resources, ctx);
        cameras.toResourceList(outData.resources, ctx.resFactory);
        users.toResourceList(outData.resources);
        layouts.toResourceList(outData.resources);

        rules.toResourceList(outData.bRules, ctx.pool);
        cameraHistory.toResourceList(outData.cameraHistory);

        foreach(const ApiResourceParam& param, kvPairs)
        {
            QnKvPairList& kvPairs = outData.kvPairs[param.resourceId];
            if( kvPairs.empty() )
                kvPairs.reserve( KV_RESOURCE_PARAMS_COMMON_COUNT );
            kvPairs << QnKvPair(param.name, param.value);
        }
    }
}
