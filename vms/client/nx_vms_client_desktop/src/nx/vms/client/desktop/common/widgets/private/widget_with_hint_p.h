// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QSize>

class QWidget;
class QWidgetPrivate;

namespace nx::vms::client::desktop {

class HintButton;

class WidgetWithHintPrivate
{
    QWidget* const q = nullptr;

public:
    WidgetWithHintPrivate(QWidget* q);
    ~WidgetWithHintPrivate();

    QString hint() const;
    void setHint(const QString& value);
    void addHintLine(const QString& value);

    int spacing() const;
    void setSpacing(int value);

    QSize minimumSizeHint(const QSize& base) const;

    void handleResize();

private:
    QWidget* parentObject();
    void updateContentsMargins();

private:
    const QScopedPointer<HintButton> m_button;
    int m_spacing = 0;
};

} // namespace nx::vms::client::desktop
