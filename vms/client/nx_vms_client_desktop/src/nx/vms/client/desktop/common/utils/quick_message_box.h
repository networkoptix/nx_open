// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/message_box.h>

namespace nx::vms::client::desktop {

class QuickMessageBox: public QObject
{
    Q_OBJECT
    Q_ENUMS(QDialogButtonBox::StandardButton QnMessageBox::Icon)
    Q_FLAGS(QDialogButtonBox::StandardButtons)

public:
    explicit QuickMessageBox(QObject* parent = nullptr);

    Q_INVOKABLE QDialogButtonBox::StandardButton exec(
        QnMessageBox::Icon icon,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
