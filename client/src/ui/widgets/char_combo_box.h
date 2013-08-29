#ifndef QN_CHAR_COMBO_BOX_H
#define QN_CHAR_COMBO_BOX_H

#include "combo_box.h"

/**
 * Combo box that is better suitable for displaying lists where most of the items
 * are characters. It fixes keyboard navigation through such combo box so that
 * it disregards <tt>QApplication::keyboardInputInterval</tt>.
 * Note that this is implemented by replacing the combo box's default item view.
 */
class QnCharComboBox: public QnComboBox {
    Q_OBJECT
    typedef QnComboBox base_type;

public:
    QnCharComboBox(QWidget *parent = NULL);
    virtual ~QnCharComboBox();
};


#endif // QN_CHAR_COMBO_BOX_H
