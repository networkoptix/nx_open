#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QtGui/QWidget>
#include <QtGui/QStandardItemModel>

#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>

namespace Ui {
    class QnPopupWidget;
}

class QnPopupWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnPopupWidget(QWidget *parent = 0);
    virtual ~QnPopupWidget();
    
    /**
     * @brief addBusinessAction     Update widget with new action received.
     * @param businessAction        Received action.
     * @return                      True if addition was successful, false otherwise.
     */
    bool addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);

signals:
    void closed(BusinessEventType::Value eventType, bool ignore);

private slots:
    void at_okButton_clicked();
    void at_eventsTreeView_clicked(const QModelIndex &index);

private:
    /**
     * @brief initWidget            Setup stored parameters and display header.
     * @param eventType             Type of the event that will be displayed in this widget instance.
     */
    void initWidget(BusinessEventType::Value eventType);

    /**
     * @brief getEventTime          Get human-readable string containing time when event occured.
     * @param eventParams           Params of the last event.
     * @return                      String formatted as hh:mm:ss
     */
    QString getEventTime(const QnBusinessParams& eventParams);

    /**
     * @brief findOrCreateItem      Find or create item in the model containing data related
     *                              to received event.
     * @param eventParams           Params of the last event.
     * @return                      Item or NULL if event resource does not exist.
     */
    QStandardItem* findOrCreateItem(const QnBusinessParams& eventParams);

    /**
     * @brief updateTreeModel       Build tree model depending of event type.
     * @param businessAction        Last received action.
     * @return                      True if update was successful, false otherwise.
     */
    bool updateTreeModel(const QnAbstractBusinessActionPtr& businessAction);

    /**
     * @brief updateSimpleTree      Used to build and update tree containing only event resources.
     * @param eventParams           Params of the last event.
     * @return                      Updated item or NULL if event resource does not exist.
     */
    QStandardItem* updateSimpleTree(const QnBusinessParams& eventParams);

    /**
     * @brief updateReasonTree      Used to build and update tree containing event resources
     *                              and event reason (for failure events).
     * @param eventParams           Params of the last event.
     * @return                      Updated item or NULL if event resource does not exist.
     */
    QStandardItem* updateReasonTree(const QnBusinessParams& eventParams);

    /**
     * @brief updateConflictTree    Used to build and update tree containing event resources
     *                              and list of conflicting entities.
     * @param eventParams           Params of the last event.
     * @return                      Updated item or NULL if event resource does not exist.
     */
    QStandardItem* updateConflictTree(const QnBusinessParams& eventParams);

private:
    QScopedPointer<Ui::QnPopupWidget> ui;

    BusinessEventType::Value m_eventType;

    QStandardItemModel* m_model;

    QStandardItem* m_showAllItem;
    bool m_showAll;
};

#endif // POPUP_WIDGET_H
