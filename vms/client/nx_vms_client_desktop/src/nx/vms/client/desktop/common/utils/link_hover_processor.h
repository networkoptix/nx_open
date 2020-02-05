#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QLabel;

namespace nx::vms::client::desktop {

/*
 * Class to dynamically change hovered/unhovered label links color and cursor.
 *  Color is changed to style::linkColor(m_label, isHovered).
 *  It uses Link and LinkVisited palette entries.
 */
class LinkHoverProcessor: public QObject
{
    Q_OBJECT

public:
    explicit LinkHoverProcessor(QLabel* parent);

private:
    enum class UpdateTime { Now, Later };
    void updateColors(UpdateTime when);
    void linkHovered(const QString& href);
    void changeLabelState(const QString& text, bool hovered);
    bool updateOriginalText();

private:
    const QPointer<QLabel> m_label;
    QString m_originalText;
    QString m_alteredText;
    QString m_hoveredLink;
};

} // namespace nx::vms::client::desktop
