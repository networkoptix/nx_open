#pragma once

template<class T>
class QVector;

namespace nx {
namespace client {
namespace desktop {

struct WearablePayload;
struct WearableUpload;
struct WearableState;

using WearablePayloadList = QVector<WearablePayload>;

} // namespace desktop
} // namespace client
} // namespace nx

