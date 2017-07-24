#include "clipboard_button.h"

#include <ui/style/skin.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace {

QString typicalText(ClipboardButton::StandardType type)
{
    switch (type)
    {
        case ClipboardButton::StandardType::copy:
            return ClipboardButton::tr("Copy", "to Clipboard");

        case ClipboardButton::StandardType::copyLong:
            return ClipboardButton::tr("Copy to Clipboard");

        case ClipboardButton::StandardType::paste:
            return ClipboardButton::tr("Paste", "from Clipboard");

        case ClipboardButton::StandardType::pasteLong:
            return ClipboardButton::tr("Paste from Clipboard");

        default:
            NX_ASSERT(false); //< Should never happen.
            return QString();
    }
}

QString typicalConfirmation(ClipboardButton::StandardType type)
{
    switch (type)
    {
        case ClipboardButton::StandardType::copy:
        case ClipboardButton::StandardType::copyLong:
            return ClipboardButton::tr("Copied", "to Clipboard");

        case ClipboardButton::StandardType::paste:
        case ClipboardButton::StandardType::pasteLong:
            return ClipboardButton::tr("Pasted", "from Clipboard");

        default:
            NX_ASSERT(false); //< Should never happen.
            return QString();
    }
}

} // namespace

ClipboardButton::ClipboardButton(StandardType type, QWidget* parent):
    ClipboardButton(typicalText(type), typicalConfirmation(type), parent)
{
}

ClipboardButton::ClipboardButton(
    const QString& text,
    const QString& confirmationText,
    QWidget* parent)
    :
    base_type(
        qnSkin->icon(lit("buttons/copy.png")),
        text,
        qnSkin->icon(lit("buttons/checkmark.png")),
        confirmationText,
        parent)
{
    setFlat(true);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
