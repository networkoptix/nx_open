#pragma once

class QLabel;

/*
 * Class to dynamically change hovered/unhovered label links color and cursor.
 *  Color is changed to style::linkColor(m_label, isHovered).
 *  It uses Link and LinkVisited palette entries.
 */
class QnLinkHoverProcessor : public QObject
{
public:
    explicit QnLinkHoverProcessor(QLabel* parent);

private:
    enum class UpdateTime { Now, Later };
    void updateColors(UpdateTime when);
    void linkHovered(const QString& href);
    void changeLabelState(const QString& text, bool hovered);
    bool updateOriginalText();

private:
    QLabel* m_label;
    QString m_originalText;
    QString m_alteredText;
    QString m_hoveredLink;
};
