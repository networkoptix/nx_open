#pragma once

#include <QtWidgets/QComboBox>

namespace nx {
namespace client {
namespace desktop {

/**
 * Combo box that is better suitable for displaying lists where most of the items
 * are characters. It fixes keyboard navigation through such combo box so that
 * it disregards <tt>QApplication::keyboardInputInterval</tt>.
 * Note that this is implemented by replacing the combo box's default item view.
 */
class CharComboBox: public QComboBox
{
    Q_OBJECT
    using base_type = QComboBox;

public:
    explicit CharComboBox(QWidget* parent = nullptr);
};

} // namespace desktop
} // namespace client
} // namespace nx
