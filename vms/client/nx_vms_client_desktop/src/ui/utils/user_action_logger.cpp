// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_action_logger.h"


#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QToolTip>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>

namespace {

bool containsSensitiveInfo(QObject* object)
{
    // TextField.Password is also 2, so it's OK to just check QLineEdit enum.
    return object->property("echoMode").toInt() == QLineEdit::Password;
}

QString getTextFromItem(QObject* object)
{
    // Check common text properties.
    static const char* kTextProperties[] = {
        "text", "currentText", "display", "label", "plainText", "logText"};
    for (const char* prop: kTextProperties)
    {
        QVariant value = object->property(prop);
        const auto stringValue = value.toString();
        if (stringValue.contains("TextBesideIcon"))
        {
            // Text assigned by QtQuick.Controls automatically. Skip it.
            continue;
        }
        if (value.isValid() && !stringValue.isEmpty())
            return stringValue;
    }

    auto possibleValue = object->property("value");
    if (possibleValue.isValid())
    {
        bool ok;
        possibleValue.toDouble(&ok);
        return ok ? QString::number(possibleValue.toDouble()) : possibleValue.toString();
    }

    bool isWindow = qobject_cast<QQuickWindow*>(object) || qobject_cast<QWindow*>(object)
        || qobject_cast<QDialog*>(object);
    auto possibleTitle = object->property("title");

    // The "title" property is often used for window titles, but the object's "title" is required.
    // Explicitly check if it's a window.
    if (possibleTitle.isValid() && !isWindow)
        return possibleTitle.toString();

    // Check tooltip only if all other properties failed.
    auto possibleTooltip = object->property("tooltip");
    if (possibleTooltip.isValid())
        return possibleTooltip.toString();

    return {};
}

void addCheckedInformation(QObject* obj, QStringList& result)
{
    if (!obj->property("checkable").toBool())
        return;
    result << "Was" << (obj->property("checked").toBool() ? "unchecked" : "checked");
}

QString getTextFromDelegateItem(QObject* object)
{
    if (!qobject_cast<QQuickItem*>(object))
        return {};

    QString delegateItemText;

    auto getText = [&](QObject* item)
    {
        if (!item)
            return;

        QVariant delegateItemVariant = item->property("delegateItem");
        if (delegateItemVariant.isValid() && delegateItemVariant.canConvert<QObject*>())
            delegateItemText = getTextFromItem(delegateItemVariant.value<QObject*>());
    };

    getText(object);

    // Usually click performed on MouseArea, so to get text from delegateItem need to check parent.
    if (delegateItemText.isEmpty())
        getText(object->parent());

    return delegateItemText;
}

QString getWindowTitle(QObject* obj)
{
    if (auto* widget = qobject_cast<QWidget*>(obj))
    {
        if (!widget->window())
            return {};
        return widget->window()->windowTitle();
    }
    if (auto* qmlItem = qobject_cast<QQuickItem*>(obj))
    {
        if (!qmlItem->window())
            return {};
        return qmlItem->window()->title();
    }
    return {};
}
} // namespace

namespace nx::vms::client::desktop {

void handleMouseButtonRelease(QObject* object, QStringList& resultString)
{
    if (auto* tabWidget = qobject_cast<QTabBar*>(object))
    {
        resultString << "Switched to tab " << tabWidget->tabText(tabWidget->currentIndex());
    }
    else if (auto* toolButton = qobject_cast<QToolButton*>(object))
    {
        resultString << "Clicked ToolButton:" << toolButton->toolTip();
    }
    else if (auto* closeButton = qobject_cast<CloseButton*>(object))
    {
        resultString << "Close button clicked";
        if (closeButton->parent())
        {
            QString parentText = getTextFromItem(closeButton->parent());
            if (!parentText.isEmpty())
                resultString << "Parent:" << parentText;
        }
    }
    else if (auto* menu = qobject_cast<QMenu*>(object))
    {
        if (QAction* selectedAction = menu->activeAction())
            resultString << "Selected menu action:" << selectedAction->text();
    }
    else
    {
        auto objectText = getTextFromItem(object);
        if (objectText.isEmpty())
            objectText = getTextFromDelegateItem(object);
        if (!objectText.isEmpty())
        {
            resultString << "Clicked on:" << objectText << "("
                         << object->metaObject()->className() << ")";
        }
    }

    // Add additional info for CheckBox, GroupBox, etc.
    addCheckedInformation(object, resultString);

    // TODO: #vbutkevich Add support for QComboBox. (QML ComboBox supported)
    // The clicked event is not processed directly on QComboBox, but on internal private
    // containers. So required some good workaround to get text of QComboBox AFTER click was performed
    // and currentText was changed.

}


void handleToolTip(QStringList& resultString)
{
    QString tooltipText = QToolTip::text();
    if (!tooltipText.isEmpty())
        resultString << "Displayed tooltip:" << tooltipText;
}


void handleFocusOut(QObject* object, QStringList& resultString)
{
    // TODO: #vbutkevich Improve to cover all text input actions.

    auto* lineEdit = qobject_cast<QLineEdit*>(object);
    auto* textEdit = qobject_cast<QTextEdit*>(object);
    auto* qmlItem = qobject_cast<QQuickItem*>(object);
    if ((lineEdit || textEdit || qmlItem) && !containsSensitiveInfo(object))
    {
        QString text = getTextFromItem(object);
        if (!text.isEmpty())
            resultString << "Set element text:" << text;
    }
}

QStringList defaultHandler(QObject* object, QEvent* event)
{
    QStringList resultString;

    switch (event->type())
    {
        case QEvent::MouseButtonRelease:
            handleMouseButtonRelease(object, resultString);
        break;

        case QEvent::ToolTip:
            handleToolTip(resultString);
        break;

        case QEvent::FocusOut:
            handleFocusOut(object, resultString);
        break;

        default:
            break;
    }

    return resultString;
}

UserActionsLogger* UserActionsLogger::instance()
{
    static UserActionsLogger sInstance;
    return &sInstance;
}

void UserActionsLogger::registerLogHandlerForClass(const QString& metatypeClassName, LogHandler handler)
{
    m_handlers[metatypeClassName] = handler;
}

bool UserActionsLogger::eventFilter(QObject* obj, QEvent* event)
{
    QStringList resultLogString;
    if (const auto& handler = m_handlers[obj->metaObject()->className()])
        resultLogString = handler(obj, event);
    else
        resultLogString = defaultHandler(obj, event);

    if (!resultLogString.isEmpty())
    {
        QString windowTitle = getWindowTitle(obj);
        if (!windowTitle.isEmpty())
            windowTitle = "Window:" + windowTitle + ".";

        NX_INFO(this, windowTitle + resultLogString.join(" "));
    }

    return QObject::eventFilter(obj, event);
}
} // namespace nx::vms::client::desktop
