
#include "read_only_event_eater.h"

#include <QtGui/QKeyEvent>


        QnReadOnlyEventEater::QnReadOnlyEventEater(QObject *parent = NULL): QnMultiEventEater(parent) {
            addEventType(QEvent::MouseButtonPress);
            addEventType(QEvent::MouseButtonRelease);
            addEventType(QEvent::MouseButtonDblClick);
            addEventType(QEvent::MouseMove);

            addEventType(QEvent::Wheel);

            addEventType(QEvent::NonClientAreaMouseButtonDblClick);
            addEventType(QEvent::NonClientAreaMouseButtonPress);
            addEventType(QEvent::NonClientAreaMouseButtonRelease);
            addEventType(QEvent::NonClientAreaMouseMove);

            addEventType(QEvent::DragEnter);
            addEventType(QEvent::DragMove);
            addEventType(QEvent::DragLeave);
            addEventType(QEvent::Drop);

            addEventType(QEvent::TabletMove);
            addEventType(QEvent::TabletPress);
            addEventType(QEvent::TabletRelease);

            addEventType(QEvent::TouchBegin);
            addEventType(QEvent::TouchEnd);
            addEventType(QEvent::TouchUpdate);

            addEventType(QEvent::KeyPress);
            addEventType(QEvent::KeyRelease);
        }

        bool QnReadOnlyEventEater::activate(QObject *, QEvent *event) {
            if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
                QKeyEvent *e = static_cast<QKeyEvent *>(event);
                if(e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)
                    return false; /* We want the tab-focus to work. */
            }

            return true;
        }

Q_DECLARE_METATYPE(QnReadOnlyEventEater *);
