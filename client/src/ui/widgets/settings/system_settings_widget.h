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

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;
    virtual void retranslateUi() override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;
private:
    QScopedPointer<Ui::SystemSettingsWidget> ui;
};
