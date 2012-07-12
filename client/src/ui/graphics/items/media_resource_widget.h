#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

class QnMediaResourceWidget: public QnResourceWidget {
    Q_OBJECT;
public:
    QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnMediaResourceWidget();

private:

};

#endif // QN_MEDIA_RESOURCE_WIDGET_H
