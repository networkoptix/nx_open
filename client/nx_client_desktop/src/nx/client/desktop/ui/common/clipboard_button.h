#pragma once

#include <nx/client/desktop/ui/common/button_with_confirmation.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ClipboardButton: public ButtonWithConfirmation
{
    Q_OBJECT
    using base_type = ButtonWithConfirmation;

public:
    enum class StandardType
    {
        copy,     //< "Copy"
        copyLong, //< "Copy to Clipboard"
        paste,    //< "Paste"
        pasteLong //< "Paste from Clipboard"
    };

    explicit ClipboardButton(StandardType type, QWidget* parent = nullptr);

    explicit ClipboardButton(const QString& text, const QString& confirmationText,
        QWidget* parent = nullptr);
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
