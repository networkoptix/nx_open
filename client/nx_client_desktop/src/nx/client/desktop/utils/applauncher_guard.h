#pragma once

#include <QtCore/QObject>

class QTimerEvent;

namespace nx {
namespace client {
namespace desktop {

/** This class keeps applaucher running. */
class ApplauncherGuard: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ApplauncherGuard(QObject* parent = nullptr);

protected:
    virtual void timerEvent(QTimerEvent* event) override;
};

} // namespace desktop
} // namespace client
} // namespace nx
