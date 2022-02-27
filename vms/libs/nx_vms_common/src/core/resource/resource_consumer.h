// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef resource_consumer_h_1921
#define resource_consumer_h_1921

#include <core/resource/resource_fwd.h>

class NX_VMS_COMMON_API QnResourceConsumer
{
public:
    explicit QnResourceConsumer(const QnResourcePtr& resource);
    virtual ~QnResourceConsumer();

    const QnResourcePtr &getResource() const;

    bool isConnectedToTheResource() const;

    virtual void beforeDisconnectFromResource() {}
    virtual void disconnectFromResource();

protected:
    QnResourcePtr m_resource;
};

#endif // resource_consumer_h_1921
