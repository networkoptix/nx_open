// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "command_action.h"

#include <optional>

#include <QtCore/QUrlQuery>
#include <QtGui/QAction>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExpression>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_loader.h>

namespace nx::vms::client::desktop {

namespace {

static const char* kCommandActionPropertyName = "__nx_commandAction";

QUrl resolvedIconUrl(const QString& iconPath)
{
    if (iconPath.isEmpty())
        return {};

    const QUrl relative(iconPath);
    NX_ASSERT(relative.isRelative());

    return QUrl("image://skin/").resolved(relative);
}

} // namespace

struct CommandAction::Private
{
    bool checkable = false;
    bool checked = false;
    bool enabled = true;
    bool visible = true;
    QString text;
    QString iconPath;
    QUrl iconUrl;
    mutable std::optional<QIcon> icon;
    QKeySequence shortcut;
};

CommandAction::CommandAction(QObject* parent):
    CommandAction(QString{}, QString{}, parent)
{
}

CommandAction::CommandAction(const QString& text, QObject* parent):
    CommandAction(text, QString{}, parent)
{
}

CommandAction::CommandAction(const QString& text, const QString& iconPath, QObject* parent):
    QObject(parent),
    d(new Private{.text = text, .iconPath = iconPath})
{
}

CommandAction::~CommandAction()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool CommandAction::enabled() const
{
    return d->enabled;
}

void CommandAction::setEnabled(bool value)
{
    if (d->enabled == value)
        return;

    d->enabled = value;
    emit enabledChanged(value);
}

bool CommandAction::visible() const
{
    return d->visible;
}

void CommandAction::setVisible(bool value)
{
    if (d->visible == value)
        return;

    d->visible = value;
    emit visibleChanged(value);
}

bool CommandAction::checkable() const
{
    return d->checkable;
}

void CommandAction::setCheckable(bool value)
{
    if (d->checkable == value)
        return;

    d->checkable = value;
    emit changed();
}

bool CommandAction::checked() const
{
    return d->checked;
}

void CommandAction::setChecked(bool value)
{
    if (d->checked == value)
        return;

    d->checked = value;
    emit changed();
}

QString CommandAction::text() const
{
    return d->text;
}

void CommandAction::setText(const QString& value)
{
    if (d->text == value)
        return;

    d->text = value;
    emit changed();
}

QIcon CommandAction::icon() const
{
    if (d->icon)
        return *d->icon;

    if (d->iconPath.isEmpty())
    {
        d->icon = {};
        return {};
    }

    const QUrl url(d->iconPath);
    if (url.path().endsWith(".svg"))
    {
        QMap<QString /*source class name*/, QString /*target color name*/> substitutions;
        for (const auto& [className, colorName]: QUrlQuery(url.query()).queryItems())
            substitutions[className] = colorName;

        d->icon = QIcon(core::loadSvgImage(":/skin/" + url.path(), substitutions, QSize{},
            qnSkin->isHiDpi() ? 2 : 1));
    }
    else
    {
        d->icon = qnSkin->icon(url.path());
    }

    return *d->icon;
}

QUrl CommandAction::iconUrl() const
{
    return d->iconUrl;
}

QString CommandAction::iconPath() const
{
    return d->iconPath;
}

void CommandAction::setIconPath(const QString& value)
{
    if (d->iconPath == value)
        return;

    d->iconPath = value;
    d->iconUrl = resolvedIconUrl(d->iconPath);
    d->icon = std::nullopt;

    emit changed();
}

QKeySequence CommandAction::shortcut() const
{
    return d->shortcut;
}

void CommandAction::setShortcut(const QKeySequence& value)
{
    if (d->shortcut == value)
        return;

    d->shortcut = value;
    emit changed();
}

void CommandAction::toggle()
{
    if (!d->enabled || !d->checkable)
        return;

    setChecked(!checked());
    emit toggled();
}

void CommandAction::trigger()
{
    if (!d->enabled)
        return;

    const QPointer<CommandAction> guard(this);
    if (d->checkable)
        toggle();

    if (guard)
        trigger();
}

QAction* CommandAction::createQtAction(const CommandActionPtr& source, QObject* parent)
{
    if (!source)
        return nullptr;

    auto action = new QAction(parent);
    action->setProperty(kCommandActionPropertyName, QVariant::fromValue(source));

    const auto updateAction =
        [action, source]()
        {
            action->setCheckable(source->checkable());
            action->setChecked(source->checked());
            action->setText(source->text());
            action->setIcon(source->icon());
            action->setShortcut(source->shortcut());
        };

    QObject::connect(source.get(), &CommandAction::enabledChanged, action, &QAction::setEnabled);
    QObject::connect(source.get(), &CommandAction::visibleChanged, action, &QAction::setVisible);
    QObject::connect(source.get(), &CommandAction::changed, action, updateAction);

    QObject::connect(action, &QAction::toggled, source.get(), &CommandAction::toggle);
    QObject::connect(action, &QAction::triggered, source.get(), &CommandAction::triggered);

    action->setEnabled(source->enabled());
    action->setVisible(source->visible());
    updateAction();

    return action;
}

QJSValue CommandAction::createQmlAction(const CommandActionPtr& source, QQmlEngine* engine)
{
    if (!source || !NX_ASSERT(engine))
        return {};

    QQmlComponent actionComponent(engine, "Nx.Controls", "Action");
    auto action = actionComponent.create();
    action->setProperty(kCommandActionPropertyName, QVariant::fromValue(source));

    const auto setActionEnabled =
        [action](bool value) { action->setProperty("enabled", value); };

    const auto setActionVisible =
        [action](bool value) { action->setProperty("visible", value); };

    const auto updateAction =
        [action, source]()
        {
            action->setProperty("checkable", source->checkable());
            action->setProperty("checked", source->checked());
            action->setProperty("text", source->text());
            action->setProperty("shortcut", source->shortcut());

            // `icon.source` cannot be assigned via `setProperty`.
            QQmlExpression(qmlContext(action), action, nx::format("icon.source = \"%1\"",
                source->iconUrl().toString())).evaluate();
        };

    QObject::connect(source.get(), &CommandAction::enabledChanged, action, setActionEnabled);
    QObject::connect(source.get(), &CommandAction::visibleChanged, action, setActionVisible);
    QObject::connect(source.get(), &CommandAction::changed, action, updateAction);

    QObject::connect(action, SIGNAL(toggled(QObject*)), source.get(), SLOT(toggle()));
    QObject::connect(action, SIGNAL(triggered(QObject*)), source.get(), SIGNAL(triggered()));

    setActionEnabled(source->enabled());
    setActionVisible(source->visible());
    updateAction();

    return engine->newQObject(action);
}

CommandActionPtr CommandAction::linkedCommandAction(QAction* qtAction)
{
    return qtAction
        ? qtAction->property(kCommandActionPropertyName).value<CommandActionPtr>()
        : CommandActionPtr{};
}

} // namespace nx::vms::client::desktop
