// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::core {

class AbstractCameraDataLoader;
using AbstractCameraDataLoaderPtr = QSharedPointer<AbstractCameraDataLoader>;

class CachingCameraDataLoader;
using CachingCameraDataLoaderPtr = QSharedPointer<CachingCameraDataLoader>;

} // namespace nx::vms::client::core
