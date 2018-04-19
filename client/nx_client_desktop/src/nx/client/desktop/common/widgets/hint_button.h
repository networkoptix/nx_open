#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <qpointer.h>
#include <qscopedpointer.h>

#include <ui/help/help_handler.h>

namespace nx {
namespace client {
namespace desktop {

/**
 * Question mark that spawns tooltip when hovered
 */
class HintButton: public QWidget
{
    using base_type = QWidget;
    Q_OBJECT;
    Q_PROPERTY(QString hint READ hint WRITE setHint)

public:
    HintButton(QWidget* parent);

    void addHintLine(QString line);
    void setHelpTopic(int topicId);
    bool isClickable() const;
    void setHint(QString hint);
    QString hint() const;

protected:
    void showTooltip(bool show);

    QSize sizeHint() const override;
    void paintEvent(QPaintEvent* event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent * event) override;
    void leaveEvent(QEvent * event) override;

protected:
    bool m_isHovered = false;
    bool m_tooltipVisible = false;
    QPixmap m_normal;
    QPixmap m_highlighted;

    int m_helpTopicId;
    QString m_hint;
    QScopedPointer<QnHelpHandler> m_helpHandler;
};

} // namespace desktop
} // namespace client
} // namespace nx
