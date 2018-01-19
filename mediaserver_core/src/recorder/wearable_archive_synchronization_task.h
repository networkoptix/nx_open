#pragma once

#include <QtCore/QIODevice>

#include <nx/mediaserver/server_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

class QnAbstractArchiveStreamReader;
class QnServerEdgeStreamRecorder;

namespace nx {
namespace mediaserver_core {
namespace recorder {

/**
 * Synchronization task for wearable cameras.
 * Basically just writes a single file into archive.
 */
class WearableArchiveSynchronizationTask: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    WearableArchiveSynchronizationTask(
        QnCommonModule* commonModule,
        const QnSecurityCamResourcePtr& resource,
        std::unique_ptr<QIODevice> file,
        qint64 startTimeMs
    );
    ~WearableArchiveSynchronizationTask();

    bool execute();

private:
    void createArchiveReader(qint64 startTimeMs);
    void createStreamRecorder(qint64 startTimeMs);

private:
    QnSecurityCamResourcePtr m_resource;
    QPointer<QIODevice> m_file;
    qint64 m_startTimeMs;

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
