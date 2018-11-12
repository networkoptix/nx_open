#pragma once

#include <QtCore/QObject>
#include <QtCore/QMargins>
#include <QtCore/QPointer>
#include <QtCore/Qt>

class QWidget;

namespace nx::vms::client::desktop {

/**
 * Common class to anchor a widget to edges of its parent.
 */
class WidgetAnchor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    /** Edges to anchor, default all anchored: */
    Q_PROPERTY(Qt::Edges edges READ edges WRITE setEdges)

    /** Margins to keep at anchored edges, default all zeros: */
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins)

public:
    explicit WidgetAnchor(QWidget* widget);

    void setEdges(Qt::Edges anchors);
    Qt::Edges edges() const;

    void setMargins(const QMargins& margins);
    void setMargins(int left, int top, int right, int bottom);
    const QMargins& margins() const;

    static constexpr Qt::Edges kAllEdges =
        Qt::Edges(Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    void updateGeometry();

private:
    Qt::Edges m_edges = WidgetAnchor::kAllEdges;
    QMargins m_margins;
    const QPointer<QWidget> m_widget;
};

/**
 * Utility function for easy usage of WidgetAnchor class.
 */
WidgetAnchor* anchorWidgetToParent(QWidget* widget,
    Qt::Edges edges = WidgetAnchor::kAllEdges,
    const QMargins& margins = QMargins());

WidgetAnchor* anchorWidgetToParent(QWidget* widget, const QMargins& margins);

} // namespace nx::vms::client::desktop
