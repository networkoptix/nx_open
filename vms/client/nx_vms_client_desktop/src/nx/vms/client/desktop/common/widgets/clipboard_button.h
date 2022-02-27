// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/button_with_confirmation.h>

class QLineEdit;

namespace nx::vms::client::desktop {

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

    explicit ClipboardButton(QWidget* parent = nullptr);

    explicit ClipboardButton(StandardType type, QWidget* parent = nullptr);

    explicit ClipboardButton(const QString& text, const QString& confirmationText,
        QWidget* parent = nullptr);

    void setType(StandardType type);

    static ClipboardButton* createInline(QLineEdit* parent, StandardType type);

    static QString clipboardText();
    static void setClipboardText(const QString& value);
};

} // namespace nx::vms::client::desktop
