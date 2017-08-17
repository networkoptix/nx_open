#include "export_settings_dialog_p.h"

#include <utils/common/delayed.h>

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

void ExportSettingsDialog::Private::exportVideo()
{
    qDebug() << "ready to export";
    executeDelayedParented([this]{setState(State::success);}, 5000, this);
    setState(State::exporting);
}

ExportSettingsDialog::Private::State ExportSettingsDialog::Private::state() const
{
    return m_state;
}

void ExportSettingsDialog::Private::setState(State value)
{
    if (value == m_state)
        return;

    m_state = value;
    emit stateChanged(value);
}

void ExportSettingsDialog::Private::loadSettings()
{
    setState(State::ready);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
