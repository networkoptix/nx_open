// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    explicit QnBookmarkBusinessActionWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~QnBookmarkBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::BookmarkBusinessActionWidget> ui;
};
