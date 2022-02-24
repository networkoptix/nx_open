// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_message_box.h"

#include <QtQml/QtQml>

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
    QDialogButtonBox::StandardButton defaultButton)
{
    // TODO: #vkutin Think how to pass a proper parent here.
    QnMessageBox msgBox(icon, text, extras, buttons, defaultButton, nullptr);
    return static_cast<QDialogButtonBox::StandardButton>(msgBox.exec());
}

void QuickMessageBox::registerQmlType()
{
    qRegisterMetaType<QnMessageBox::Icon>();
    qRegisterMetaType<QDialogButtonBox::StandardButton>();
    qRegisterMetaType<QDialogButtonBox::StandardButtons>();

    qmlRegisterSingletonType<QuickMessageBox>("nx.vms.client.desktop", 1, 0, "MessageBox",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            static QuickMessageBox instance;
            qmlEngine->setObjectOwnership(&instance, QQmlEngine::CppOwnership);
            return &instance;
        });
}

} // namespace nx::vms::client::desktop
