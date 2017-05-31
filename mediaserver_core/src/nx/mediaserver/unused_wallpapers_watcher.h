#pragma once

#include <QObject>
#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <nx_ec/data/api_stored_file_data.h>

class QnCommonModule;

namespace nx {
namespace mediaserver {

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
    std::vector<ec2::ApiStoredFilePath> m_previousData;
};

} // namespace mediaserver
} // namespace nx
