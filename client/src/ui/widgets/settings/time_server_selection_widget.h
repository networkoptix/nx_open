#ifndef QN_TIME_SERVER_SELECTION_WIDGET_H
#define QN_TIME_SERVER_SELECTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class TimeServerSelectionWidget;
}

class QnTimeServerSelectionModel;

class QnTimeServerSelectionWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnTimeServerSelectionWidget(QWidget *parent = NULL);
    virtual ~QnTimeServerSelectionWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

private:
    void updateTime();

    QUuid selectedServer() const;

private:
    QScopedPointer<Ui::TimeServerSelectionWidget> ui;
    QnTimeServerSelectionModel* m_model;
};

#endif // QN_TIME_SERVER_SELECTION_WIDGET_H
