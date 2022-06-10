// Copyright 2022-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

namespace nx::vms::client::desktop {

/**
 * Draws a hint in addition to the main item text. The hint color can be different from the main
 * item color.
 */
class ComboBoxWithHint: public QComboBox
{
    Q_OBJECT
    using base_type = QComboBox;
public:
    explicit ComboBoxWithHint(QWidget* parent = nullptr);
    
    void setItemHint(int index, const QString& text);

    void setHintColor(const QColor& color);
    QColor hintColor() const;

    void setHintRole(int value);
    int hintRole() const;

protected:
    virtual QSize sizeHint() const override;

private:
    QColor m_hintColor;
    int m_hintRole = Qt::UserRole;
};

} // namespace nx::vms::client::desktop
