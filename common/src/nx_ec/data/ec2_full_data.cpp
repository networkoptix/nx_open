#include "ec2_full_data.h"
#include "core/resource/media_server_resource.h"

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
        outData.kvPairs.reserve(kvPairs.size());
        foreach(const ApiResourceParam& param, kvPairs)
            outData.kvPairs << QnKvPair(param.name, param.value);
    }
}
