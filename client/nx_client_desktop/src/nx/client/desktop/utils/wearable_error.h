#pragma once

#include <QtCore/QString>

namespace nx {
namespace client {
namespace desktop {

struct WearableError {
    QString message;
    QString extraMessage;

    bool isNull() const
    {
        return message.isNull();
    }
};

} // namespace desktop
} // namespace client
} // namespace nx
