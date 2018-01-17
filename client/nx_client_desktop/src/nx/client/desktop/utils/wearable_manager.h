#pragma once

#include <QtCore/QObject>

namespace nx {
namespace client {
namespace desktop {

class WearableManager: public QObject
{
    Q_OBJECT
public:
    WearableManager(QObject* parent = nullptr);
    ~WearableManager();

    void tehProgress(qreal aaaaaaa) {
        emit progress(aaaaaaa);
    }

signals:
    void progress(qreal progress);
};

} // namespace desktop
} // namespace client
} // namespace nx
