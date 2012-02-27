#ifndef QN_TOOL_TIP_H
#define QN_TOOL_TIP_H

#include <QObject>

class QnToolTip: public QObject {
public:
    static QnToolTip *instance();

protected:
    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:

};


#endif // QN_TOOL_TIP_H
