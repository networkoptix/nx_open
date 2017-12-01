#include "clipboard_button.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

#include <nx/utils/log/assert.h>
#include <ui/style/skin.h>

#include <nx/client/desktop/ui/common/line_edit_controls.h>

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

ClipboardButton::ClipboardButton(QWidget* parent):
    ClipboardButton(StandardType::copy, parent)
{
}

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
        qnSkin->icon(lit("text_buttons/copy.png")),
        text,
        qnSkin->icon(lit("buttons/checkmark.png")),
        confirmationText,
        parent)
{
}

ClipboardButton* ClipboardButton::createInline(QLineEdit* parent, StandardType type)
{
    auto button = new ClipboardButton(type, parent);
    button->setCursor(Qt::ArrowCursor);
    button->setFocusPolicy(Qt::NoFocus);
    LineEditControls::get(parent)->addControl(button);

    switch (type)
    {
        case StandardType::copy:
        case StandardType::copyLong:
        {
            const auto onTextChanged =
                [button](const QString& text) { button->setHidden(text.isEmpty()); };

            connect(parent, &QLineEdit::textChanged, onTextChanged);

            connect(button, &QPushButton::clicked,
                [parent]() { qApp->clipboard()->setText(parent->text()); });

            onTextChanged(parent->text());
            break;
        }

        case StandardType::paste:
        case StandardType::pasteLong:
        {
            const auto onClipboardChanged =
                [button]() { button->setEnabled(!qApp->clipboard()->text().isEmpty()); };

            connect(qApp->clipboard(), &QClipboard::dataChanged, onClipboardChanged);

            connect(button, &QPushButton::clicked,
                [parent]() { parent->setText(qApp->clipboard()->text()); });

            onClipboardChanged();
            break;
        }
    }

    return button;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
