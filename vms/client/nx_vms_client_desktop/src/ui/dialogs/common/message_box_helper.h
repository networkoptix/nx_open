// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QDialogButtonBox>

template<typename MessageBox>
class QnMessageBoxHelper: public MessageBox
{
    using base_type = MessageBox;

public:
    using base_type::base_type;

public:
    static QDialogButtonBox::StandardButton information(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton)
    {
        return execute(
            parent, QnMessageBoxIcon::Information, text, extras, buttons, defaultButton);
    }

    static QDialogButtonBox::StandardButton warning(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton)
    {
        return execute(parent, QnMessageBoxIcon::Warning, text, extras, buttons, defaultButton);
    }

    static QDialogButtonBox::StandardButton question(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton)
    {
        return execute(parent, QnMessageBoxIcon::Question, text, extras, buttons, defaultButton);
    }

    static QDialogButtonBox::StandardButton critical(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton)
    {
        return execute(parent, QnMessageBoxIcon::Critical, text, extras, buttons, defaultButton);
    }

    static QDialogButtonBox::StandardButton success(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton)
    {
        return execute(parent, QnMessageBoxIcon::Success, text, extras, buttons, defaultButton);
    }

private:
    static QDialogButtonBox::StandardButton execute(
        QWidget* parent,
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
    {
        MessageBox messageBox(parent);
        messageBox.setIcon(icon);
        messageBox.setText(text);
        messageBox.setInformativeText(extras);
        messageBox.setStandardButtons(buttons);
        messageBox.setDefaultButton(defaultButton);

        return static_cast<QDialogButtonBox::StandardButton>(messageBox.exec());
    }
};
