#ifndef __KV_PAIRS_H_
#define __KV_PAIRS_H_

namespace ec2 {
    struct ApiResourceParamData;
    struct ApiResourceParamWithRefData;
    typedef std::vector<ApiResourceParamData> ApiResourceParamDataList;
    typedef std::vector<ApiResourceParamWithRefData> ApiResourceParamWithRefDataList;
};
#endif
