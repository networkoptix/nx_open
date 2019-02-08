#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>

class QnCommonModule;

namespace nx {
namespace vms::server {

/**
 * Monitor unused layout's wallpapers in the database and remove it.
 */
class UnusedWallpapersWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT;
    using base_type = QnCommonModuleAware;
public:
    UnusedWallpapersWatcher(QnCommonModule* commonModule);
    void start();
public slots:
    void update();
private:
    QTimer m_timer;
    QStringList m_previousData;
};

} // namespace mediaserverm_previousData
} // namespace nx
