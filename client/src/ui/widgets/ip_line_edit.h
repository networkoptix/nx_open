#ifndef IP_LINE_EDIT_H
#define IP_LINE_EDIT_H

#include <QtGui/QLineEdit>

class QnIpLineEdit: public QLineEdit{
    Q_OBJECT

public:
    explicit QnIpLineEdit(QWidget* parent=0);
protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
};

#endif // IP_LINE_EDIT_H
