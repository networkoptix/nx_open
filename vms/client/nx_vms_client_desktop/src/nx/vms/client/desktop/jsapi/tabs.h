// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "tab.h"

namespace nx::vms::client::desktop {

class WindowContext;

namespace jsapi {

/**
 * Contains methods to work with multiple tabs.
 */
class Tabs: public QObject
{
    Q_OBJECT

    /**
     * @addtogroup vms-tabs
     * Tabs object. Contains methods to work with multiple tabs.
     *
     * Example:
     *
     *     let tab = vms.tabs.tabs.find(tab => tab.name === "Example")
     *
     *     if (!tab)
     *       tab = await vms.tabs.add("Example")
     *
     *     vms.tabs.setCurrent(tab)
     *
     * @{
     */

    /**
     * All tabs.
     */
    Q_PROPERTY(QList<Tab*> tabs READ tabs NOTIFY tabsChanged)

    /**
     * Current tab.
     */
    Q_PROPERTY(Tab* current READ current NOTIFY currentChanged CONSTANT)

public:
    /** @private */
    Tabs(WindowContext* context, QObject* parent = nullptr);

    /** @private */
    virtual ~Tabs() override;

    /** @private */
    QList<Tab*> tabs() const;

    /** @private */
    Tab* current() const;

    /**
     * Sets the current tab.
     */
    Q_INVOKABLE QJsonObject setCurrent(Tab* tab);

    /**
     * Sets the current tab.
     */
    Q_INVOKABLE QJsonObject setCurrent(const QString& id);

    /**
     * Adds a tab.
     * @param name Must be unique and not empty.
     * @returns Tab object if added, null otherwise.
     */
    Q_INVOKABLE Tab* add(const QString& name);

    /**
     * Removes the tab.
     */
    Q_INVOKABLE QJsonObject remove(Tab* tab);

    /**
     * Removes the tab.
     */
    Q_INVOKABLE QJsonObject remove(const QString& id);

    /**
     * Opens a layout in a new tab.
     */
    Q_INVOKABLE QJsonObject open(const QString& layoutResourceId);

signals:
    void tabsChanged();
    void currentChanged();

    /**
     * Current Tab signal, for convenience.
     */
    void currentTabItemAdded(const QJsonObject& item);

    /**
     * Current Tab signal, for convenience.
     */
    void currentTabItemRemoved(const nx::Uuid& itemId);

    /**
     * Current Tab signal, for convenience.
     */
    void currentTabItemChanged(const QJsonObject& item);

    /** @} */ // group vms-tabs

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace jsapi
} // namespace nx::vms::client::desktop
