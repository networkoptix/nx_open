#ifndef BOOKMARK_BUSINESS_ACTION_WIDGET_H
#define BOOKMARK_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class BookmarkBusinessActionWidget;
}

class QnBookmarkBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnBookmarkBusinessActionWidget(QWidget *parent = 0);
    ~QnBookmarkBusinessActionWidget();
private:
    QScopedPointer<Ui::BookmarkBusinessActionWidget> ui;
};

#endif // BOOKMARK_BUSINESS_ACTION_WIDGET_H
