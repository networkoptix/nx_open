// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "tab.h"

class QnWorkbenchItem;

namespace nx::vms::client::desktop { class WindowContext; }

namespace nx::vms::client::desktop::jsapi {

/**
 * Self object provides functionality related to the current web page item. Note that this object
 * is unavailable if the web page is opened in a dialog.
 */
class Self: public QObject
{
    Q_OBJECT

    /**
     * @ingroup vms-self
     * Tab that contains current web page.
     */
    Q_PROPERTY(Tab* tab READ tab CONSTANT)

    /**
     * @ingroup vms-self
     * Current web page item id.
     */
    Q_PROPERTY(nx::Uuid itemId READ itemId CONSTANT)

public:
    Self(QnWorkbenchItem* item, QObject* parent = nullptr);
    virtual ~Self() override;

    /**
     * @addtogroup vms-self
     * Contains methods to control the current web-page widget.
     * @{
    */

    /**
     * Sets the web-page widget minimal interface mode. In this mode only the close button is
     * visible.
     */
    Q_INVOKABLE Error setMinimalInterfaceMode(bool value);

    /**
     * Prevents showing the default context menu.
     */
    Q_INVOKABLE Error setPreventDefaultContextMenu(bool value);

    /** @} */ // group self

    Tab* tab() const;
    nx::Uuid itemId() const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::jsapi
