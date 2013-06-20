
#ifndef READ_ONLY_EVENT_EATER_H
#define READ_ONLY_EVENT_EATER_H

#include <QObject>

#include <utils/common/event_processors.h>


    class QnReadOnlyEventEater: public QnMultiEventEater {

        Q_OBJECT
    public:
        QnReadOnlyEventEater(QObject *parent = NULL);

    protected:
        virtual bool activate(QObject *, QEvent *event) override;
    };

#endif	//READ_ONLY_EVENT_EATER_H
