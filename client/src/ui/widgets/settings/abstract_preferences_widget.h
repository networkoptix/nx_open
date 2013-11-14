#ifndef ABSTRACT_PREFERENCES_WIDGET_H
#define ABSTRACT_PREFERENCES_WIDGET_H

#include <QtWidgets/QWidget>

class QnAbstractPreferencesWidget: public QWidget {
    Q_OBJECT
public:
    explicit QnAbstractPreferencesWidget(QWidget *parent = 0):
        QWidget(parent) {}

    /**
     * @brief submitToSettings                  Save widget elements' values to client settings.
     */
    virtual void submitToSettings() {}

    /**
     * @brief updateFromSettings                Read widget elements' values from client settings.
     */
    virtual void updateFromSettings() {}

    /**
     * @brief confirm                           Check that all values are correct so saving is possible.
     * @return                                  False if saving should be aborted, true otherwise.
     */
    virtual bool confirm() {return true;}
};

#endif // ABSTRACT_PREFERENCES_WIDGET_H
