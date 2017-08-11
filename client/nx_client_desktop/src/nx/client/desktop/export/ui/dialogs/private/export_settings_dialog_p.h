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
    explicit Private(QObject* parent = nullptr);
    virtual ~Private() override;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx