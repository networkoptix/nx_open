// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class DatabaseManagementWidget;
}

class QnDatabaseManagementWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnDatabaseManagementWidget(QWidget *parent = nullptr);
    virtual ~QnDatabaseManagementWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

protected:
    void setReadOnlyInternal(bool readOnly) override;

private:
    void backupDb();
    void restoreDb();

private:
    QScopedPointer<Ui::DatabaseManagementWidget> ui;
};
