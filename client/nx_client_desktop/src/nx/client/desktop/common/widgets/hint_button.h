#pragma once

#include <QtWidgets/QWidget>
#include <QPointer>

class QGroupBox;

namespace nx {
namespace client {
namespace desktop {

/**
 * Question mark that spawns tooltip when hovered.
 * It can be attached to QGroupBox and appear right after its caption.
 */
class HintButton: public QWidget
{
    using base_type = QWidget;
    Q_OBJECT;
    Q_PROPERTY(QString hint READ hint WRITE setHint)

public:
    HintButton(QWidget* parent);

    // Create hint button, that is attached to box's header.
    static HintButton* hintThat(QGroupBox* groupBox);

    void addHintLine(QString line);
    void setHelpTopic(int topicId);
    bool isClickable() const;
    void setHint(QString hint);
    QString hint() const;
    // Returns prefered size from internal pixmap.
    QSize hintMarkSize() const;
    QSize sizeHint() const override;
    // Updates geometry, relative to current groupBox state.
    void updateGeometry(QGroupBox* parent);

protected:
    void showTooltip(bool show);
    virtual void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

protected:
    bool m_isHovered = false;
    bool m_tooltipVisible = false;
    QPixmap m_normal;
    QPixmap m_highlighted;

    QPointer<QGroupBox> m_parentBox;
    int m_helpTopicId;
    QString m_hint;
};

} // namespace desktop
} // namespace client
} // namespace nx
