#include "ec2_full_data.h"
#include "core/resource/media_server_resource.h"


static const int KV_RESOURCE_PARAMS_COMMON_COUNT = 10;

namespace ec2
{
    void ApiFullInfo::toResourceList(QnFullResourceData& outData, const ResourceContext& ctx) const
    {
        fromApiToResourceTypeList(resTypes, outData.resTypes);
        foreach(const QnResourceTypePtr& resType, outData.resTypes)
            const_cast<QnResourceTypePool*>(ctx.resTypePool)->addResourceType(resType); // todo: refactor it!

        servers.toResourceList(outData.resources, ctx);
        cameras.toResourceList(outData.resources, ctx.resFactory);
        users.toResourceList(outData.resources);
        layouts.toResourceList(outData.resources);
        videowalls.toResourceList(outData.resources);
        licenses.toResourceList(outData.licenses);

        outData.bRules = rules.toResourceList(ctx.pool);
        cameraHistory.toResourceList(outData.cameraHistory);
        outData.serverInfo = serverInfo;
    }
}
