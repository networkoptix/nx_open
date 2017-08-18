#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>

#include <utils/common/delayed.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsDialog::Private::Private(QObject* parent):
    base_type(parent)
{

}

ExportSettingsDialog::Private::~Private()
{

}

void ExportSettingsDialog::Private::loadSettings()
{
}

void ExportSettingsDialog::Private::setMediaResource(const QnMediaResourcePtr& media)
{
    m_exportMediaSettings.mediaResource = media;
}

void ExportSettingsDialog::Private::setTimePeriod(const QnTimePeriod& period)
{
    m_exportMediaSettings.timePeriod = period;
    m_exportLayoutSettings.period = period;
}

void ExportSettingsDialog::Private::setFilename(const QString& filename)
{
    m_exportMediaSettings.fileName = filename;
    m_exportLayoutSettings.filename = filename;
}

ExportSettingsDialog::Private::ErrorCode ExportSettingsDialog::Private::status() const
{
    return m_status;
}

bool ExportSettingsDialog::Private::isExportAllowed(ErrorCode code)
{
    switch (code)
    {
        case ErrorCode::ok:
            return true;

        default:
            return false;
    }
}

ExportSettingsDialog::Mode ExportSettingsDialog::Private::mode()
{
    return m_mode;
}

const ExportMediaSettings& ExportSettingsDialog::Private::exportMediaSettings() const
{
    return m_exportMediaSettings;
}

const ExportLayoutSettings& ExportSettingsDialog::Private::exportLayoutSettings() const
{
    return m_exportLayoutSettings;
}

void ExportSettingsDialog::Private::setStatus(ErrorCode value)
{
    if (value == m_status)
        return;

    m_status = value;
    emit statusChanged(value);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
