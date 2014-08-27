#ifndef QN_TIME_SERVER_SELECTION_WIDGET_H
#define QN_TIME_SERVER_SELECTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

namespace Ui {
    class TimeServerSelectionWidget;
}

class QnTimeServerSelectionWidget: public QnAbstractPreferencesWidget {
    Q_OBJECT
public:
    QnTimeServerSelectionWidget(QWidget *parent = NULL);
    virtual ~QnTimeServerSelectionWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

private:
    QScopedPointer<Ui::TimeServerSelectionWidget> ui;
};

#endif // QN_TIME_SERVER_SELECTION_WIDGET_H
