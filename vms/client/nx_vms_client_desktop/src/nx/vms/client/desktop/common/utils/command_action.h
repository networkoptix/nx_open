// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>
#include <QtGui/QIcon>
#include <QtGui/QKeySequence>
#include <QtQml/QJSValue>

#include <nx/utils/impl_ptr.h>

class QAction;
class QQmlEngine;

namespace nx::vms::client::desktop {

class CommandAction;
using CommandActionPtr = QSharedPointer<CommandAction>;
using CommandActionList = QList<CommandActionPtr>;

class CommandAction: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool checkable READ checkable WRITE setCheckable NOTIFY changed)
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QString iconPath READ iconPath WRITE setIconPath NOTIFY changed)
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY changed)

public:
    explicit CommandAction(QObject* parent = nullptr);
    explicit CommandAction(const QString& text, QObject* parent = nullptr);
    explicit CommandAction(const QString& text, const QString& iconPath, QObject* parent = nullptr);
    virtual ~CommandAction() override;

    bool enabled() const;
    void setEnabled(bool value);

    bool visible() const;
    void setVisible(bool value);

    bool checkable() const;
    void setCheckable(bool value);

    bool checked() const;
    void setChecked(bool value);

    QString text() const;
    void setText(const QString& value);

    QIcon icon() const;
    QUrl iconUrl() const;
    QString iconPath() const;
    void setIconPath(const QString& value);

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence& value);

    Q_SLOT void toggle();
    Q_SLOT void trigger();

    static QAction* createQtAction(const CommandActionPtr& source, QObject* parent = nullptr);
    static QJSValue createQmlAction(const CommandActionPtr& source, QQmlEngine* engine);

    static CommandActionPtr linkedCommandAction(QAction* qtAction);

signals:
    void triggered();
    void toggled();

    void enabledChanged(bool value);
    void visibleChanged(bool value);
    void changed();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
