#pragma once

#include <QtWidgets/QAbstractButton>
#include <QtCore/QPointer>

class QGroupBox;

namespace nx::vms::client::desktop {

/**
 * Question mark that spawns tooltip when hovered.
 * It can be attached to QGroupBox and appear right after its caption.
 * It gets ID of help topic using utilities from help_topic_accessor.h
 */
class HintButton: public QAbstractButton
{
    using base_type = QAbstractButton;
    Q_OBJECT;
    Q_PROPERTY(QString hint READ hint WRITE setHint)

public:
    HintButton(QWidget* parent = nullptr);
    HintButton(const QString& hint, QWidget* parent = nullptr);

    // Create hint button, that is attached to box's header.
    static HintButton* hintThat(QGroupBox* groupBox);

    void addHintLine(QString line);
    bool hasHelpTopic() const;
    void setHint(QString hint);
    QString hint() const;
    // Returns prefered size from internal pixmap.
    QSize hintMarkSize() const;
    virtual QSize sizeHint() const override;
    // Updates geometry, relative to current groupBox state.
    void updateGeometry(QGroupBox* parent);

protected:
    void showTooltip(bool show);
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void enterEvent(QEvent* event) override;
    virtual  void leaveEvent(QEvent* event) override;
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

    int getHelpTopicId() const;

protected:
    bool m_isHovered = false;
    bool m_tooltipVisible = false;
    QPixmap m_normal;
    QPixmap m_highlighted;
    QString m_hint;
    // Pointer to a widget we are embedded to.
    // We need to track its state.
    QPointer<QGroupBox> m_hostBox;
};

} // namespace nx::vms::client::desktop
