// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_RESOURCE_PROCESSOR_H
#define QN_RESOURCE_PROCESSOR_H

#include "resource_fwd.h"

class QnResourceProcessor
{
public:
    virtual ~QnResourceProcessor() {}

    virtual void processResources(const QnResourceList &resources) = 0;
    virtual bool isBusy() const { return false; }
};

#endif // QN_RESOURCE_PROCESSOR_H
