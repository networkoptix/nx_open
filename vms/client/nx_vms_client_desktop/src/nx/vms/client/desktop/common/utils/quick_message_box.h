// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/message_box.h>

namespace nx::vms::client::desktop {

class QuickMessageBox: public QObject
{
    Q_OBJECT
    Q_ENUMS(QDialogButtonBox::StandardButton QDialogButtonBox::ButtonRole QnMessageBox::Icon)
    Q_FLAGS(QDialogButtonBox::StandardButtons)

public:
    explicit QuickMessageBox(QObject* parent = nullptr);

    Q_INVOKABLE QDialogButtonBox::StandardButton exec(
        QnMessageBox::Icon icon,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        const QJsonObject& warningButton = {},
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton,
        const QString& windowTitle = {});

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
