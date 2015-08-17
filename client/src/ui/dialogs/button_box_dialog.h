#ifndef QN_BUTTON_BOX_DIALOG_H
#define QN_BUTTON_BOX_DIALOG_H


#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/dialog_base.h>
#include <utils/common/connective.h>

/**
 * Button box dialog that can be queried for the button that was clicked to close it.
 */
class QnButtonBoxDialog: public Connective<QnDialogBase> {
    Q_OBJECT;
    typedef Connective<QnDialogBase> base_type;

public:
    QnButtonBoxDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnButtonBoxDialog();

    QDialogButtonBox::StandardButton clickedButton() const {
        return m_clickedButton;
    }
    
protected:
    void setButtonBox(QDialogButtonBox *buttonBox);

    virtual bool event(QEvent *event) override;

    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button);
private:
    void initializeButtonBox();

    Q_SLOT void at_buttonBox_clicked(QAbstractButton *button);

private:
    QDialogButtonBox::StandardButton m_clickedButton;
    QPointer<QDialogButtonBox> m_buttonBox;
};

#endif // QN_BUTTON_BOX_DIALOG_H
