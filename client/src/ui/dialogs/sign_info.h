#ifndef __SIGN_INGO_H__
#define __SIGN_INGO_H__

#include <QLabel>

class QnSignInfo: public QLabel
{
    Q_OBJECT
public:
    QnSignInfo(QWidget* parent = 0);
public slots:
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
    void at_calcSignInProgress(QByteArray sign, int progress);
};

#endif // __SIGN_INGO_H__
