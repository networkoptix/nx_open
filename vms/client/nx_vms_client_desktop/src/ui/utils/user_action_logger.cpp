// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_action_logger.h"

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTextEdit>
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
    if (!object)
        return {};

    // Check common text properties.
    static const char* kTextProperties[] = {
        "text", "currentText", "display", "label", "plainText", "logText"};
    for (const char* prop: kTextProperties)
    {
        const QVariant value = object->property(prop);
        const auto stringValue = value.toString();
        if (stringValue.contains("TextBesideIcon"))
        {
            // Text assigned by QtQuick.Controls automatically. Skip it.
            continue;
        }
        if (value.isValid() && !stringValue.isEmpty())
            return stringValue;
    }

    const auto possibleValue = object->property("value");
    if (possibleValue.isValid())
    {
        bool ok;
        possibleValue.toDouble(&ok);
        return ok ? QString::number(possibleValue.toDouble()) : possibleValue.toString();
    }

    const bool isWindow = qobject_cast<QQuickWindow*>(object) || qobject_cast<QWindow*>(object)
        || qobject_cast<QDialog*>(object);
    const auto possibleTitle = object->property("title");

    // The "title" property is often used for window titles, but the object's "title" is required.
    // Explicitly check if it's a window.
    if (possibleTitle.isValid() && !isWindow)
        return possibleTitle.toString();

    // Check tooltip only if all other properties failed.
    const auto possibleTooltip = object->property("tooltip");
    if (possibleTooltip.isValid())
        return possibleTooltip.toString();

    return {};
}

void addMetaObjectInfo(QObject* object, QStringList& result)
{
    if (result.empty())
        return;

    QStringList additionalInfo;

    if (!object->objectName().isEmpty())
        additionalInfo << "Object name (" << object->objectName() << ")";
    else
        additionalInfo << "Class name (" << object->metaObject()->className() << ")";

    result << additionalInfo.join(' ');
}

void addCheckedInformation(QObject* obj, QStringList& result)
{
    QStringList additionalInfo;
    if (!obj->property("checkable").toBool())
        return;

    additionalInfo << "Was" << (obj->property("checked").toBool() ? "unchecked" : "checked");
    result << additionalInfo.join(' ');
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

void handleMouseButtonRelease(QObject* object, QStringList& resultLog)
{
    QStringList initialInfo;
    if (auto* tabWidget = qobject_cast<QTabBar*>(object))
    {
        initialInfo << "Switched to tab " << tabWidget->tabText(tabWidget->currentIndex());
    }
    else if (auto* closeButton = qobject_cast<CloseButton*>(object))
    {
        initialInfo << "Close button clicked";
        if (closeButton->parent())
        {
            QString parentText = getTextFromItem(closeButton->parent());
            if (!parentText.isEmpty())
                initialInfo << "Parent:" << parentText;
        }
    }
    else if (auto* menu = qobject_cast<QMenu*>(object))
    {
        if (QAction* selectedAction = menu->activeAction())
            initialInfo << "Selected menu action:" << selectedAction->text();
    }
    else
    {
        auto objectText = getTextFromItem(object);
        if (objectText.isEmpty())
            objectText = getTextFromDelegateItem(object);
        if (!objectText.isEmpty())
            initialInfo << "Clicked on:" << objectText;
    }

    if (!initialInfo.isEmpty())
        resultLog.append(initialInfo.join(' '));
    // Add additional info for CheckBox, GroupBox, etc.
    addCheckedInformation(object, resultLog);

    // For some objects, the metaobject info can contain useful information (e.g., buttons on the
    // Timeline).
    addMetaObjectInfo(object, resultLog);
}

/** Workaround to handle selection in QComboBox. Triggered when the internal popup view is about
 * to lose focus, which happens after the user selects an item via mouse or keyboard.
 */
void handleFocusAboutToChange(QObject* object, QStringList& resultString)
{
    const bool grandparentIsComboBox = object->parent() && object->parent()->parent()
        && qobject_cast<QComboBox*>(object->parent()->parent());

    if (!grandparentIsComboBox)
        return;

    const auto* itemView = qobject_cast<QAbstractItemView*>(object);
    if (!itemView || !itemView->selectionModel())
        return;

    const QModelIndex index = itemView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    const QString text = index.data(Qt::DisplayRole).toString();
    if (!text.isEmpty())
        resultString << "Selected item in ComboBox:" + text;
}

void handleToolTip(QStringList& resultString)
{
    const QString tooltipText = QToolTip::text();
    if (!tooltipText.isEmpty())
        resultString << "Displayed tooltip:" + tooltipText;
}

void handleKeyPress(QKeyEvent* keyPressEvent, QStringList& resultString)
{
    if (!keyPressEvent)
        return;

    const QString keySequence =
        QKeySequence(keyPressEvent->modifiers(), keyPressEvent->key()).toString();

    if (!keySequence.isEmpty())
        resultString << "Key combination pressed:" + keySequence;
}

void handleShortcut(QShortcutEvent* shortcutEvent, QStringList& resultString)
{
    if (!shortcutEvent)
        return;
    const QString combination = shortcutEvent->key().toString();
    if (!combination.isEmpty())
        resultString << "Shortcut triggered:" + combination;
}

void handleFocusOut(QObject* object, QStringList& resultString)
{
    // TODO: #vbutkevich Improve to cover all text input actions.

    auto* lineEdit = qobject_cast<QLineEdit*>(object);
    auto* textEdit = qobject_cast<QTextEdit*>(object);
    auto* qmlItem = qobject_cast<QQuickItem*>(object);
    if ((lineEdit || textEdit || qmlItem) && !containsSensitiveInfo(object))
    {
        const QString text = getTextFromItem(object);
        if (!text.isEmpty())
            resultString << "Set element text:" + text;
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

        case QEvent::Shortcut:
            handleShortcut(static_cast<QShortcutEvent*>(event), resultString);
            break;

        case QEvent::KeyPress:
            handleKeyPress(static_cast<QKeyEvent*>(event), resultString);
            break;

        case QEvent::FocusAboutToChange:
            handleFocusAboutToChange(object, resultString);
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
    QStringList resultLogInfo;
    if (const auto& handler = m_handlers[obj->metaObject()->className()])
        resultLogInfo = handler(obj, event);
    else
        resultLogInfo = defaultHandler(obj, event);

    if (!resultLogInfo.isEmpty())
    {
        QString windowTitle = getWindowTitle(obj);
        if (!windowTitle.isEmpty())
            windowTitle = "Window:" + windowTitle + ".";

        NX_INFO(this, windowTitle + resultLogInfo.join("."));
    }

    return QObject::eventFilter(obj, event);
}
} // namespace nx::vms::client::desktop
