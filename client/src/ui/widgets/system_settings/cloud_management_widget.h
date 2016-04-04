#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace Ui
{
    class CloudManagementWidget;
}

class QnCloudManagementWidget: public Connective<QnAbstractPreferencesWidget>, QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    QnCloudManagementWidget(QWidget *parent = nullptr);
    virtual ~QnCloudManagementWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;

private:
    QScopedPointer<Ui::CloudManagementWidget> ui;
};
