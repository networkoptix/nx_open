// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

namespace nx::vms::client::desktop {

class GlobalToolTipAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool showOnHover READ showOnHover WRITE setShowOnHover
        NOTIFY showOnHoverChanged)
    Q_PROPERTY(bool stickToItem READ stickToItem WRITE setStickToItem
        NOTIFY stickToItemChanged)
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)

public:
    GlobalToolTipAttached(QObject* parent);
    virtual ~GlobalToolTipAttached() override;

    QString text() const;
    void setText(const QString& value);

    bool isEnabled() const;
    void setEnabled(bool value);

    bool showOnHover() const;
    void setShowOnHover(bool value);

    bool stickToItem() const; //< Whether the tooltip moves if it's invoker item moves.
    void setStickToItem(bool value);

    int delay() const;
    void setDelay(int value);

    Q_INVOKABLE void show(const QString& text, int delay = -1);
    Q_INVOKABLE void showTemporary(const QString& temporaryText, int durationMs = 1000);
    Q_INVOKABLE void hide();

signals:
    void textChanged();
    void enabledChanged();
    void showOnHoverChanged();
    void stickToItemChanged();
    void delayChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

class GlobalToolTip: public QObject
{
    Q_OBJECT

public:
    explicit GlobalToolTip(QObject* parent = nullptr);

    static GlobalToolTipAttached* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::GlobalToolTip, QML_HAS_ATTACHED_PROPERTIES)
