// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/property_storage/storage.h>

#include "control_bars.h"

class QHBoxLayout;
class QPushButton;
class QLayout;

namespace nx::vms::client::desktop {

struct BarDescription
{
    using PropertyPointer = QPointer<nx::utils::property_storage::Property<bool>>;
    enum class BarLevel
    {
        Info,
        Warning,
        Error
    };

    /** Text of banner's label. */
    QString text;
    /** Level of alert, which sets the background color and icon of banner. */
    BarLevel level = BarLevel::Info;
    /**
     * Is close button required in the right corner.
     * Note, that isClosable has value true if the isEnabledProperty is set.
     */
    bool isClosable = false;
    /** Is opening of external links allowed in Banner's label. */
    bool isOpenExternalLinks = true;
    /** Pointer to property from MessageBarSettings to check, if banner should be shown or not. */
    PropertyPointer isEnabledProperty;
    bool operator==(const BarDescription&) const = default;
};

/**
 * A text message on a colored background.
 * It's intended to be positioned in an external layout at a top or a bottom of a dialog.
 * Has appearance depending on type CommonMessageBar::BarLevel
 */
class CommonMessageBar: public ControlBar
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = ControlBar;

public:
    CommonMessageBar(QWidget* parent = nullptr, const BarDescription& description = {});
    virtual ~CommonMessageBar() override;

    QString text() const;
    void setText(const QString& text);

    void init(const BarDescription& barDescription);
    void updateVisibility();
    void hideInFuture();

    /** Enabled by default. */
    void setOpenExternalLinks(bool open);

    /**
     * Creates button with required style, adds it to children of banner,
     * adds it to the banner's layout.
     */
    QPushButton* addButton(const QString& text, const QString& iconPath);
    /** Adds existing button to layout of banner and set the style of button. */
    void addButton(QPushButton* button);

signals:
    void closeClicked();
    /** Emitted only when `setOpenExternalLinks` is disabled. */
    void linkActivated(const QString& link);

private:
    using ControlBar::verticalLayout;

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Automatically generated vertical set of CommonMessageBars.
 */
class MessageBarBlock: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;
public:
    explicit MessageBarBlock(QWidget* parent = nullptr);
    virtual ~MessageBarBlock() override;
    void setMessageBars(const std::vector<BarDescription>& descs);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
