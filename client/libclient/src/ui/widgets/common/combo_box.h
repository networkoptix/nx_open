#ifndef QN_COMBO_BOX_H
#define QN_COMBO_BOX_H

#include <QtWidgets/QComboBox>

/**
 * A combo box with additional slots that are not available in <tt>QComboBox</tt>.
 */
class QnComboBox: public QComboBox {
    Q_OBJECT
    typedef QComboBox base_type;

public:
    QnComboBox(QWidget *parent = NULL): base_type(parent) {}

public slots:
    virtual void showPopup() override { base_type::showPopup(); }
    virtual void hidePopup() override { base_type::hidePopup(); }
};


#endif // QN_COMBO_BOX_H

