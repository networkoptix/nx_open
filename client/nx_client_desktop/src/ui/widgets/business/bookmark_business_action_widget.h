#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui {
    class BookmarkBusinessActionWidget;
}

class QnBookmarkBusinessActionWidget : public QnSubjectTargetActionWidget
{
    Q_OBJECT
    typedef QnSubjectTargetActionWidget base_type;

public:
    explicit QnBookmarkBusinessActionWidget(QWidget *parent = 0);
    ~QnBookmarkBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();

private:
    void updateUserSelectionControls();

private:
    QScopedPointer<Ui::BookmarkBusinessActionWidget> ui;
};
