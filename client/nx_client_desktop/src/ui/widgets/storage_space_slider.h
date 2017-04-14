#pragma once

#include <QtWidgets/QSlider>

class QEvent;
class QMouseEvent;
class QPaintEvent;

/**
 * A slider that displays the space to be used on a storage.
 * Slider value is expected to be in MiBs.
 *
 * This slider was used as an editor widget in server settings dialog.
 * Currently it is not used anywhere, but we want to keep it.
 */
class QnStorageSpaceSlider: public QSlider
{
    Q_OBJECT
    using base_type = QSlider;

public:
    QnStorageSpaceSlider(QWidget* parent = nullptr);

    const QColor& color() const;
    void setColor(const QColor& color);

    QString text() const;
    QString textFormat() const;
    void setTextFormat(const QString& textFormat);

    static QString formatSize(qint64 size);

protected:
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

private:
    qreal relativePosition() const;
    int handlePos() const;
    bool isEmpty() const;

private:
    QColor m_color;
    QString m_textFormat;
    bool m_textFormatHasPlaceholder;
};
