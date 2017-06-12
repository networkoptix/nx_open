#ifndef IP_LINE_EDIT_H
#define IP_LINE_EDIT_H

#include <QtWidgets/QLineEdit>

class QnIpLineEdit: public QLineEdit{
    Q_OBJECT
typedef QLineEdit base_type;

public:
    explicit QnIpLineEdit(QWidget* parent=0);
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
};

#endif // IP_LINE_EDIT_H
