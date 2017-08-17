#pragma once

#include <QtCore/QObject>

#include <nx/client/desktop/export/ui/dialogs/export_settings_dialog.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ExportSettingsDialog::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class State
    {
        invalid,
        ready,
        exporting,
        error,
        success
    };

    explicit Private(QObject* parent = nullptr);
    virtual ~Private() override;

    void exportVideo();

    void loadSettings();

    State state() const;

signals:
    void stateChanged(State value);

private:
    void setState(State value);

private:
    State m_state = State::invalid;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx