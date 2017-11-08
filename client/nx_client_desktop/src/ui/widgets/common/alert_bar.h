#pragma once

#include <QtWidgets/QWidget>

class QLabel;
class QHBoxLayout;

class QnAlertBar: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = QWidget;

public:
    QnAlertBar(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);

    /**
     * This property controls how the bar should behave when no text is set.
     * If reservedSpace is set, bar will be displayed without text and color - just take space.
     * It is used when we don't want to change the dialog size.
     */
    bool reservedSpace() const;
    void setReservedSpace(bool reservedSpace);

    QHBoxLayout* getOverlayLayout();

private:
    void updateVisibility();

private:
    QLabel* m_label = nullptr;
    QHBoxLayout* m_layout = nullptr;
    bool m_reservedSpace = false;
};

/* This class is a helper for the easy customization option. */
class QnPromoBar: public QnAlertBar
{
    Q_OBJECT
public:
    QnPromoBar(QWidget* parent = nullptr): QnAlertBar(parent) {}
};
