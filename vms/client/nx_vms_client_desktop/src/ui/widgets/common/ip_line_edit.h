#pragma once
#include <QtWidgets/QLineEdit>

class QnIpLineEdit: public QLineEdit
{
    Q_OBJECT
typedef QLineEdit base_type;

public:
    explicit QnIpLineEdit(QWidget* parent=0);
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
};

