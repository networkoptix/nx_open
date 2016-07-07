#include "char_combo_box.h"
#include "private/char_combo_box_p.h"

QnCharComboBox::QnCharComboBox(QWidget *parent):
    base_type(parent)
{
    setView(new QnCharComboBoxListView(this));
}

QnCharComboBox::~QnCharComboBox() {
    return;
}
