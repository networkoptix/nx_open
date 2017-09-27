#pragma once

#include <QtWidgets/QComboBox>

namespace nx {
namespace client {
namespace desktop {

// A combo box with separate width control of line edit and popup parts,
// and ability to override text in line edit when in non-editable mode.

class AdvancedComboBox: public QComboBox
{
    Q_OBJECT
    using base_type = QComboBox;

    Q_PROPERTY(int popupWidth READ popupWidth WRITE setPopupWidth)
    Q_PROPERTY(int customTextRole READ customTextRole WRITE setCustomTextRole)
    Q_PROPERTY(QString customText READ customText WRITE setCustomText)
    Q_PROPERTY(Qt::TextElideMode textElideMode READ textElideMode WRITE setTextElideMode)

public:
    explicit AdvancedComboBox(QWidget* parent = nullptr);
    virtual ~AdvancedComboBox() override;

    // Fixed width of combo box popup. Not set if zero or negative,
    // then the width is determined by sizeHint. Not set by default.
    int popupWidth() const;
    void setPopupWidth(int value);

    // Line edit text override. Not set if null string QString(), empty string if lit("").
    // Ignored if editable == true.
    QString customText() const;
    void setCustomText(const QString& value);

    // Item data role to fetch line edit text override. Not set if negative.
    // Has lower priority than customText. Ignored if editable == true.
    int customTextRole() const;
    void setCustomTextRole(int value);

    // Line edit text elide mode. Qt::ElideNone by default.
    // Irrelevant if editable == true.
    Qt::TextElideMode textElideMode() const;
    void setTextElideMode(Qt::TextElideMode value);

    virtual void showPopup() override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

    int effectivePopupWidth() const;

private:
    int m_popupWidth = 0;
    Qt::TextElideMode m_textElideMode = Qt::ElideNone;
    QString m_customText;
    int m_customTextRole = -1;
};

} // namespace desktop
} // namespace client
} // namespace nx
