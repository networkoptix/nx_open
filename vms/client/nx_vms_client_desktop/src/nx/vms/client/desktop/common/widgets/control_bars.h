// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <ui/widgets/word_wrapped_label.h>

class QHBoxLayout;
class QVBoxLayout;

namespace nx::vms::client::desktop {

class ControlBar: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ControlBar(QWidget* parent = nullptr);
    virtual ~ControlBar() override;

    bool retainSizeWhenHidden() const;
    void setRetainSizeWhenHidden(bool value);

    QVBoxLayout* verticalLayout() const;
    QHBoxLayout* horizontalLayout() const;

    bool isDisplayed() const;
    void setDisplayed(bool value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

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

    QHBoxLayout* overlayLayout() const;
    QnWordWrappedLabel* label() const;

private:
    using base_type::setDisplayed;

signals:
    /** Emitted only when `setOpenExternalLinks` is disabled. */
    void linkActivated(const QString& link);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

// The following classes are helpers for easy customization.

class AlertBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    explicit AlertBar(QWidget* parent = nullptr);
};

class PromoBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    using base_type::base_type; //< Forward constructors.
};

// TODO: #v4.2 Unify multiline message bars with the ones that reserve space.
class MultilineAlertBar: public QnWordWrappedLabel
{
    Q_OBJECT
    using base_type = QnWordWrappedLabel;

public:
    explicit MultilineAlertBar(QWidget* parent = nullptr);
};

class AlertBlock: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    AlertBlock(QWidget* parent = nullptr);

    void setAlerts(const QStringList& value);

private:
    QList<AlertBar*> m_bars;
    QVBoxLayout* const m_layout = nullptr;
};

} // namespace nx::vms::client::desktop
