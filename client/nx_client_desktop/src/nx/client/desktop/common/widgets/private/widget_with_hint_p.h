#pragma once

#include <QtCore/QScopedPointer>

class QWidget;
class QWidgetPrivate;

namespace nx {
namespace client {
namespace desktop {

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
    void updateContentsMargins();

private:
    const QScopedPointer<HintButton> m_button;
    int m_spacing = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx
