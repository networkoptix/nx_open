#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

namespace Ui {
    class SystemSettingsWidget;
}

class QnSystemSettingsWidget: public QnAbstractPreferencesWidget {
    Q_OBJECT
public:
    QnSystemSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSystemSettingsWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

private:
    QScopedPointer<Ui::SystemSettingsWidget> ui;
};
