// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_message_box.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/qml/qml_ownership.h>

namespace nx::vms::client::desktop {

QuickMessageBox::QuickMessageBox(QObject* parent):
    QObject(parent)
{
}

QDialogButtonBox::StandardButton QuickMessageBox::exec(
    QnMessageBox::Icon icon,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    const QJsonObject& warningButton,
    QDialogButtonBox::StandardButton defaultButton,
    const QString& windowTitle)
{
    // TODO: #vkutin Think how to pass a proper parent here.
    QnMessageBox msgBox(icon, text, extras, buttons, defaultButton, nullptr);

    if (!warningButton.empty())
    {
        msgBox.addButton(
            warningButton["text"].toString(),
            static_cast<QDialogButtonBox::ButtonRole>(warningButton["role"].toInt()),
            Qn::ButtonAccent::Warning);
    }

    if (!windowTitle.isNull())
        msgBox.setWindowTitle(windowTitle);

    return static_cast<QDialogButtonBox::StandardButton>(msgBox.exec());
}

void QuickMessageBox::registerQmlType()
{
    qRegisterMetaType<QnMessageBox::Icon>();
    qRegisterMetaType<QDialogButtonBox::StandardButton>();
    qRegisterMetaType<QDialogButtonBox::StandardButtons>();
    qRegisterMetaType<QDialogButtonBox::ButtonRole>();

    qmlRegisterSingletonType<QuickMessageBox>("nx.vms.client.desktop", 1, 0, "MessageBox",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            static QuickMessageBox instance;
            return core::withCppOwnership(&instance);
        });
}

} // namespace nx::vms::client::desktop
