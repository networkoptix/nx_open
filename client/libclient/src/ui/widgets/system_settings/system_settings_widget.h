#pragma once

#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui
{
    class SystemSettingsWidget;
}

class QnSystemSettingsWidget: public QnAbstractPreferencesWidget, public QnConnectionContextAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnSystemSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSystemSettingsWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;
    virtual void retranslateUi() override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    QScopedPointer<Ui::SystemSettingsWidget> ui;
};
