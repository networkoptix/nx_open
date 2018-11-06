#pragma once

template<class T>
class QVector;

namespace nx::vms::client::desktop {

struct WearablePayload;
struct WearableUpload;
struct WearableState;

using WearablePayloadList = QVector<WearablePayload>;

} // namespace nx::vms::client::desktop

