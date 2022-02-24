// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtWidgets/QWidget>

#include <ui/widgets/word_wrapped_label.h>

class QHBoxLayout;
class QVBoxLayout;

namespace nx::vms::client::desktop {

class MessageBar: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = QWidget;

public:
    explicit MessageBar(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);

    /** Enabled by default. */
    void setOpenExternalLinks(bool open);

    /**
     * This property controls how the bar should behave when no text is set.
     * If reservedSpace is set, bar will be displayed without text and color - just take space.
     * It is used when we don't want to change the dialog size.
     */
    bool reservedSpace() const;
    void setReservedSpace(bool reservedSpace);

    QVBoxLayout* mainLayout() const;
    QHBoxLayout* overlayLayout() const;
    QnWordWrappedLabel* label() const;

signals:
    /** Emitted only when `setOpenExternalLinks` is disabled. */
    void linkActivated(const QString& link);

private:
    void updateVisibility();

private:
    QWidget* const m_background = nullptr;
    QnWordWrappedLabel* const m_label = nullptr;
    QVBoxLayout* const m_layout = nullptr;
    QHBoxLayout* const m_overlayLayout = nullptr;
    bool m_reservedSpace = false;
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
