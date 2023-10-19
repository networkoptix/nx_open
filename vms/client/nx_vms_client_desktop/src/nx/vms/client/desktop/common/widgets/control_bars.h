// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <ui/widgets/word_wrapped_label.h>

class QHBoxLayout;
class QVBoxLayout;

namespace nx::vms::client::desktop {

/**
 * A block of controls on a colored background.
 * It's intended to be positioned in an external layout at a top or a bottom of a dialog.
 * Controls should be added to either verticalLayout() or horizontalLayout().
 */
class ControlBar: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ControlBar(QWidget* parent = nullptr);
    virtual ~ControlBar() override;

    /** A vertical layout to insert controls to. */
    virtual QVBoxLayout* verticalLayout() const;

    /**
     * A horizontal layout to insert controls to.
     * Occupies the first position in the vertical layout.
     */
    QHBoxLayout* horizontalLayout() const;

    /**
     * A horizontal layout, which contains vertical layout.
     */
    QHBoxLayout* mainLayout() const;

    /**
     * Whether the bar should keep occupying its space in an exterior layout when not displayed.
     */
    bool retainSpaceWhenNotDisplayed() const;
    void setRetainSpaceWhenNotDisplayed(bool value);

    /**
     * When retainSpaceWhenNotDisplayed is true, visibility must be controlled via these methods.
     * It's OK to do it in any case, even when retainSpaceWhenNotDisplayed is false.
     */
    bool isDisplayed() const;
    void setDisplayed(bool value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
