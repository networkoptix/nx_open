// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <nx/utils/singleton.h>
#include <core/resource/resource_factory.h>

namespace nx::vms::common::test_support {

class NX_VMS_COMMON_TEST_SUPPORT_API TestResourceFactory:
    public QObject,
    public QnResourceFactory
{
public:
    TestResourceFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;
};

} // namespace nx::vms::common::test_support
