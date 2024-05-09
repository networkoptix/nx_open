// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_camera_resource_stub.h"

namespace {

const nx::Uuid kThumbCameraTypeId("{72d232d7-0c67-4d8e-b5a8-a0d5075ff3a4}");

} // namespace

namespace nx::vms::client::desktop::test {

ClientCameraResourceStub::ClientCameraResourceStub()
    :
    base_type(kThumbCameraTypeId)
{
    setIdUnsafe(nx::Uuid::createUuid());
    setForceUsingLocalProperties();
}

bool ClientCameraResourceStub::setProperty(
    const QString& key,
    const QString& value,
    bool markDirty)
{
    base_type::setProperty(key, value, markDirty);
    emitPropertyChanged(key, QString(), value);
    return false;
}

} // namespace nx::vms::client::desktop::test
