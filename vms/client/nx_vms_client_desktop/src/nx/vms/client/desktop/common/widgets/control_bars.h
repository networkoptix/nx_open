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

/**
 * A text message on a colored background.
 * It's intended to be positioned in an external layout at a top or a bottom of a dialog.
 */
class MessageBar: public ControlBar
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = ControlBar;

public:
    explicit MessageBar(QWidget* parent = nullptr);
    virtual ~MessageBar() override;

    QString text() const;
    void setText(const QString& text);

    /** Enabled by default. */
    void setOpenExternalLinks(bool open);

    /** A label displaying the text message. */
    QnWordWrappedLabel* label() const;

    /**
     * An additional layout on top of the label.
     * Overlay controls may be inserted in it.
     */
    QHBoxLayout* overlayLayout() const;

    bool closeButtonVisible() const;
    void setCloseButtonVisible(bool visible);

private:
    using base_type::setDisplayed;

signals:
    /** Emitted only when `setOpenExternalLinks` is disabled. */
    void linkActivated(const QString& link);

    void closeClicked();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * A message bar customized with a red background to display error or warning messages.
 */
class AlertBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    explicit AlertBar(QWidget* parent = nullptr);
};

/**
 * A message bar customized to display promotional messages.
 */
class PromoBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    using base_type::base_type; //< Forward constructors.
};

/**
 * Automatically generated vertical set of AlertBars.
 */
class AlertBlock: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit AlertBlock(QWidget* parent = nullptr);
    void setAlerts(const QStringList& value);

private:
    QList<AlertBar*> m_bars;
    QVBoxLayout* const m_layout = nullptr;
};

// TODO: #v4.2 Unify multiline message bars with the ones that reserve space.
class MultilineAlertBar: public QnWordWrappedLabel
{
    Q_OBJECT
    using base_type = QnWordWrappedLabel;

public:
    explicit MultilineAlertBar(QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
