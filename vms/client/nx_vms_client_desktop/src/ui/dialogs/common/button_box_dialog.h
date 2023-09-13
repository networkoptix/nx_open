// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_BUTTON_BOX_DIALOG_H
#define QN_BUTTON_BOX_DIALOG_H

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/dialog.h>

/**
 * Button box dialog that can be queried for the button that was clicked to close it.
 */
class QnButtonBoxDialog: public QnDialog {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    typedef QnDialog base_type;

public:
    QnButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags = {});
    virtual ~QnButtonBoxDialog();

    QDialogButtonBox::StandardButton clickedButton() const {
        return m_clickedButton;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

protected:
    QDialogButtonBox* buttonBox() const;
    void setButtonBox(QDialogButtonBox *buttonBox);

    virtual bool event(QEvent *event) override;

    virtual void buttonBoxClicked(QAbstractButton* button);
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button);

    virtual void initializeButtonBox();

    virtual void setReadOnlyInternal();

private:
    Q_SLOT void at_buttonBox_clicked(QAbstractButton *button);

private:
    QDialogButtonBox::StandardButton m_clickedButton;
    QPointer<QDialogButtonBox> m_buttonBox;
    bool m_readOnly;
};

#endif // QN_BUTTON_BOX_DIALOG_H
