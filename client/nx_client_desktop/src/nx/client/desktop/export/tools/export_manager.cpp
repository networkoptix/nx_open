#include "export_manager.h"

#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/tools/export_media_tool.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

class ExportMediaProcess: public ExportProcess
{
public:
    ExportMediaProcess(const ExportMediaSettings& settings, QObject* parent):
        ExportProcess(parent),
        m_tool(new ExportMediaTool(settings, this))
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
                NX_EXPECT(m_info.progressValue == value, "Value not in range");
                emit infoChanged(m_info);
            });

        connect(m_tool, &ExportMediaTool::finished, this,
            [this](bool success, const QString& filename)
            {
                m_info.status = success
                    ? ExportProcessStatus::success
                    : ExportProcessStatus::error;
                qDebug() << "finished export to" << filename << "with status" <<
                    QnStreamRecorder::errorString(m_tool->status());

                emit infoChanged(m_info);
                emit finished(m_info.id);
            });
    }

    const ExportProcessInfo& info() const
    {
        return m_info;
    }

    void start()
    {
        m_info.status = ExportProcessStatus::exporting;
        emit infoChanged(m_info);

        m_tool->start();
    }

private:
    ExportMediaTool* m_tool;
};

} // namespace

ExportProcess::ExportProcess(QObject* parent):
    base_type(parent)
{
}

struct ExportManager::Private
{


    QMap<QnUuid, QPointer<ExportMediaProcess>> exportMediaProcesses;

};

ExportManager::ExportManager(QObject* parent):
    base_type(parent),
    d(new Private())
{

}

QnUuid ExportManager::exportMedia(const ExportMediaSettings& settings)
{
    auto process = new ExportMediaProcess(settings, this);
    connect(process, &ExportMediaProcess::infoChanged, this, &ExportManager::processUpdated);
    connect(process, &ExportMediaProcess::finished, this,
        [this](const QnUuid& id)
        {
            if (auto process = d->exportMediaProcesses.take(id))
                process->deleteLater();
        });
    d->exportMediaProcesses.insert(process->info().id, process);
    process->start();

    return process->info().id;
}

} // namespace desktop
} // namespace client
} // namespace nx

