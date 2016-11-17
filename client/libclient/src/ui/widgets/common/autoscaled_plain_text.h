#pragma once

#include <QtWidgets/QWidget>

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

    QString text() const;
    void setText(const QString& text);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    bool weightScaled() const; //< if font weight is inversely scaled with font size
    void setWeightScaled(bool value);

    QFont effectiveFont() const;

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnAutoscaledPlainText);
    QScopedPointer<QnAutoscaledPlainTextPrivate> d_ptr;
};
