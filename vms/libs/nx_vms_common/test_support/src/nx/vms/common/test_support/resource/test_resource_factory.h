// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_factory.h>
#include <nx/utils/singleton.h>

namespace nx::vms::common::test {

class NX_VMS_COMMON_TEST_SUPPORT_API TestResourceFactory:
    public QObject,
    public QnResourceFactory
{
public:
    TestResourceFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(
        const nx::Uuid& resourceTypeId,
        const QnResourceParams& params) override;
};

} // namespace nx::vms::common::test
