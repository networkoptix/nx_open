#ifndef QN_DATA_PROVIDER_FACTORY_H
#define QN_DATA_PROVIDER_FACTORY_H

#include <core/resource/resource_fwd.h>

#include <nx/streaming/abstract_stream_data_provider.h>

class QnDataProviderFactory {
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(const QnResourcePtr& res, Qn::ConnectionRole role) = 0;
};

#endif // QN_DATA_PROVIDER_FACTORY_H
