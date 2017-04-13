#pragma once

#include <QtWidgets/QWidget>

/**
* A widget displaying static plain text that downscales if necessary.
* QWidget::font determines desired (maximum) size of text.
* If text doesn't fit in content rectangle a smaller font is chosen
* automatically.
*/
class QnAutoscaledPlainTextPrivate;
class QnAutoscaledPlainText: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool weightScaled READ weightScaled WRITE setWeightScaled)
    Q_PROPERTY(QFont effectiveFont READ effectiveFont)

    using base_type = QWidget;

public:
    QnAutoscaledPlainText(QWidget* parent = nullptr);
    virtual ~QnAutoscaledPlainText();

    /** Displayed text: */
    QString text() const;
    void setText(const QString& text);

    /** Text alignment (Qt::AlignCenter by default): */
    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    /** If weightScaled is false, effective font always has the same weight as font.
    * If weightScaled is true (default), the more downscaled effective font is,
    * the more weight it gains (25 QFont::weight points per 2õ downscale): */
    bool weightScaled() const; //< if font weight is inversely scaled with font size
    void setWeightScaled(bool value);

    /** Currently used font: */
    QFont effectiveFont() const;

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnAutoscaledPlainText);
    QScopedPointer<QnAutoscaledPlainTextPrivate> d_ptr;
};
