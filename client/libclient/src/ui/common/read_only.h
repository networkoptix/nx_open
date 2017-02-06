#ifndef QN_READ_ONLY_H
#define QN_READ_ONLY_H

class QWidget;

void setReadOnly(QWidget *widget, bool readOnly);
bool isReadOnly(const QWidget *widget);

#endif // QN_READ_ONLY_H
