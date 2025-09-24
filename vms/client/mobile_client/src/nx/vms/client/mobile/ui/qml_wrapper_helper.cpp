// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_wrapper_helper.h"

#include <QtCore/QEventLoop>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/window_context.h>

#include "detail/qml_result_wait.h"

namespace nx::vms::client::mobile {

QString QmlWrapperHelper::showPopup(
    WindowContext* context,
    const QUrl& source,
    QVariantMap properties)
{
    const auto engine = appContext()->qmlEngine();
    QQmlComponent component(engine, source);

    const auto parentItem = context->mainWindow()->contentItem();
    properties.insert("parent", QVariant::fromValue(parentItem));

    const auto popup = component.createWithInitialProperties(properties);
    if (!popup)
        return {};

    QMetaObject::invokeMethod(popup, "open");

    QEventLoop loop;
    const auto popupMetaObject = popup->metaObject();
    const auto closedSignalIndex = popupMetaObject->indexOfSignal("closed()");
    const auto closedSignal = popupMetaObject->method(closedSignalIndex);

    const auto loopMetaObject = loop.metaObject();
    const auto quitSlotIndex = loopMetaObject->indexOfSlot("quit()");
    const auto quitSlot = loopMetaObject->method(quitSlotIndex);

    QObject::connect(popup, closedSignal, &loop, quitSlot);
    loop.exec();

    return popup->property("result").toString();
}

QString QmlWrapperHelper::showScreen(
    WindowContext* context,
    const QUrl& source,
    const QVariantMap& properties)
{
    auto stackView = context->mainWindow()->findChild<QObject*>(
        "mainStackView");
    QObject* screen = nullptr;
    QMetaObject::invokeMethod(stackView, "pushScreen", Qt::DirectConnection,
        Q_RETURN_ARG(QObject*, screen),
        Q_ARG(QUrl, source),
        Q_ARG(QVariant, QVariant::fromValue(properties)));

    return screen
        ? detail::QmlResultWait(screen).result()
        : QString{};
}

} // namespace nx::vms::client::mobile
