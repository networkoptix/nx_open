// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

/**
 * @class UserActionsLogger
 * @brief Logs user interactions with UI elements.
 *
 * The UserActionsLogger class monitors UI events, such as clicks and text changes,
 * and logs meaningful user actions. It supports custom handlers for different UI components.
 */
namespace nx::vms::client::desktop {

class UserActionsLogger: public QObject
{
    Q_OBJECT

public:
    using LogHandler = std::function<QStringList(QObject* object, QEvent* event)>;

    static UserActionsLogger* instance();
    void registerLogHandlerForClass(const QString& metatypeClassName, LogHandler handler);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    explicit UserActionsLogger(QObject* parent = nullptr): QObject(parent) {}
    std::unordered_map<QString, LogHandler> m_handlers;
};

} // namespace nx::vms::client::desktop
