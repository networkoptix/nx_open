#include "export_manager.h"

#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/export/tools/export_media_tool.h>
#include "export_layout_tool.h"

namespace nx::vms::client::desktop {

ExportProcess::ExportProcess(const QnUuid& id, std::unique_ptr<AbstractExportTool>&& tool, QObject* parent):
    base_type(parent),
    m_info(id),
    m_tool(std::move(tool))
{
    connect(m_tool, &ExportMediaTool::rangeChanged, this,
        [this](int from, int to)
        {
            m_info.rangeStart = from;
            m_info.rangeEnd = to;
            m_info.progressValue = qBound(from, m_info.progressValue, to);
            emit infoChanged(m_info);
        });

    connect(m_tool, &ExportMediaTool::valueChanged, this,
        [this](int value)
        {
            m_info.progressValue = qBound(m_info.rangeStart, value, m_info.rangeEnd);
            emit infoChanged(m_info);
        });

    connect(m_tool, &ExportMediaTool::statusChanged, this,
        [this](ExportProcessStatus status)
        {
            m_info.status = status;
            m_info.error = m_tool->lastError();
            emit infoChanged(m_info);
        });

    connect(m_tool, &ExportMediaTool::finished, this,
        [this]
        {
            emit finished(m_info.id);
        });
}

ExportProcess::~ExportProcess()
{
}

const ExportProcessInfo& ExportProcess::info() const
{
    return m_info;
}

void ExportProcess::start()
{
    m_tool->start();
}

void ExportProcess::stop()
{
    m_tool->stop();
}

struct ExportManager::Private
{
    ExportManager* const q;
    QMap<QnUuid, QPointer<ExportProcess>> exportProcesses;

    explicit Private(ExportManager* owner):
        q(owner)
    {
    }

    QnUuid startExport(ExportProcess* process)
    {
        connect(process, &ExportProcess::infoChanged, q, &ExportManager::processUpdated);
        connect(process, &ExportProcess::finished, q,
            [this](const QnUuid& id)
            {
                if (auto process = exportProcesses.take(id))
                {
                    emit q->processFinished(process->info());
                    process->deleteLater();
                }
            });
        exportProcesses.insert(process->info().id, process);
        process->start();
        return process->info().id;
    }

    void stopExport(const QnUuid& exportProcessId)
    {
        if (const auto process = exportProcesses.value(exportProcessId))
            process->stop();
    }

    ExportProcessInfo info(const QnUuid& exportProcessId) const
    {
        if (const auto process = exportProcesses.value(exportProcessId))
            return process->info();
        return ExportProcessInfo(QnUuid());
    }
};

ExportManager::ExportManager(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

ExportManager::~ExportManager()
{
}

QnUuid ExportManager::startExport(const QnUuid& id, std::unique_ptr<AbstractExportTool>&& tool)
{
    return d->startExport(new ExportProcess(id, std::move(tool), this));
}

void ExportManager::stopExport(const QnUuid& exportProcessId)
{
    d->stopExport(exportProcessId);
}

ExportProcessInfo ExportManager::info(const QnUuid& exportProcessId) const
{
    return d->info(exportProcessId);
}

QString ExportProcess::errorString(ExportProcessError error)
{
    // TODO: #gdm Better texts.
    switch (error)
    {
        case ExportProcessError::noError:
            return QString();

        case ExportProcessError::unsupportedMedia:
            return tr("Unsupported media for data export.");

        case ExportProcessError::unsupportedFormat:
            return tr("Selected format is not supported by FFMPEG library.");

        case ExportProcessError::ffmpegError:
            return tr("FFMPEG library error.");

        case ExportProcessError::incompatibleCodec:
            return tr("Video or audio codec is incompatible with selected format.");

        case ExportProcessError::fileAccess:
            return tr("File write error.");

        case ExportProcessError::dataNotFound:
            return tr("No data exported.");

        default:
            NX_ASSERT(false, "Should never get here");
            return tr("Internal error");
    }
}

} // namespace nx::vms::client::desktop

