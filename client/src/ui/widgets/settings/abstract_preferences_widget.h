#ifndef ABSTRACT_PREFERENCES_WIDGET_H
#define ABSTRACT_PREFERENCES_WIDGET_H

#include <QtWidgets/QWidget>

class QnAbstractPreferencesWidget: public QWidget {
    Q_OBJECT
public:
    explicit QnAbstractPreferencesWidget(QWidget *parent = 0):
        QWidget(parent) {}

    virtual void submitToSettings() {}
    virtual void updateFromSettings() {}

    virtual bool confirm() {return true;}
};

#endif // ABSTRACT_PREFERENCES_WIDGET_H
