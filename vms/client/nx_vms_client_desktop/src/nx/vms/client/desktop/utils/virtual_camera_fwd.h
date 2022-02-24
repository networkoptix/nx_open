// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

template<class T>
class QVector;

namespace nx::vms::client::desktop {

struct VirtualCameraPayload;
struct VirtualCameraUpload;
struct VirtualCameraState;

using VirtualCameraPayloadList = QVector<VirtualCameraPayload>;

} // namespace nx::vms::client::desktop

