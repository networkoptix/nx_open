#pragma once

/*
 * Class to dynamically change hovered links color.
 *  Unhovered link color is QApplication::palette().color(QPalette::Link) - default Qt behavior
 *  Hovered link color is m_label->color(QPalette::Link) - this class implements it
 */
class QnLinkHoverProcessor : public QObject
{
public:
    explicit QnLinkHoverProcessor(QLabel* parent);

protected:
    virtual QColor hoveredColor() const;

private:
    void linkHovered(const QString& href);
    auto labelChangeFunctor(const QString& text, bool hovered);

private:
    QLabel* m_label;
    QString m_originalText;
    bool m_textAltered;
};
