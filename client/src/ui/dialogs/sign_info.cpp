#include "sign_info.h"

QnSignInfo::QnSignInfo(QWidget* parent): QLabel(parent)
{

}


void QnSignInfo::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
    bool matched = calculatedSign == signFromFrame;
    matched = matched;
}

void QnSignInfo::at_calcSignInProgress(QByteArray sign, int progress)
{

}
