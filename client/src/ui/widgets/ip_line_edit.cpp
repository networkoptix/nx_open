#include "ip_line_edit.h"

class IpAddressValidator: QValidator{

};


QnIpLineEdit::QnIpLineEdit(QWidget *parent):
    QLineEdit(parent){
    setInputMask(QLatin1String("009.009.009.009;_"));
  //  setValidator(new IpAddressValidator());
}
