// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

/**
* A widget displaying static plain text that downscales if necessary.
* QWidget::font determines desired (maximum) size of text.
* If text doesn't fit in content rectangle a smaller font is chosen
* automatically.
*/
class AutoscaledPlainText: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool weightScaled READ weightScaled WRITE setWeightScaled)
    Q_PROPERTY(QFont effectiveFont READ effectiveFont)

    using base_type = QWidget;

public:
    AutoscaledPlainText(QWidget* parent = nullptr);
    virtual ~AutoscaledPlainText();

    /** Displayed text: */
    QString text() const;
    void setText(const QString& text);

    /** Text alignment (Qt::AlignCenter by default): */
    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    /** If weightScaled is false, effective font always has the same weight as font.
    * If weightScaled is true (default), the more downscaled effective font is,
    * the more weight it gains (25 QFont::weight points per 2� downscale): */
    bool weightScaled() const; //< if font weight is inversely scaled with font size
    void setWeightScaled(bool value);

    /** Currently used font: */
    QFont effectiveFont() const;

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
