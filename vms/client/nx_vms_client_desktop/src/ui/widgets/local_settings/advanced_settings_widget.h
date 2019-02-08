#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class AdvancedSettingsWidget;
}

class QnAdvancedSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnAdvancedSettingsWidget(QWidget *parent = 0);
    ~QnAdvancedSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

    bool isRestartRequired() const;

private slots:
    void at_browseLogsButton_clicked();
    void at_clearCacheButton_clicked();
    void at_resetAllWarningsButton_clicked();

private:
    bool isAudioDownmixed() const;
    void setAudioDownmixed(bool value);

    bool isDoubleBufferingEnabled() const;
    void setDoubleBufferingEnabled(bool value);

    bool isBlurEnabled() const;
    void setBlurEnabled(bool value);

    int  maximumLiveBufferMs() const;
    void setMaximumLiveBufferMs(int value);

private:
    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
};
