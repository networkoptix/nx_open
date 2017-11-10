#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class AbstractAnalyticsDriver: public QObject
{
    Q_OBJECT

public:
    explicit AbstractAnalyticsDriver(QObject* parent = nullptr);

signals:
    void regionAddedOrChanged(const QnUuid& id, const QRectF& region);
    void regionRemoved(const QnUuid& id);
};

} // namespace desktop
} // namespace client
} // namespace nx
