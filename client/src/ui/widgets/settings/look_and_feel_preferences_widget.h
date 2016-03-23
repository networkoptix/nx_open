#ifndef LOOK_AND_FEEL_PREFERENCES_WIDGET_H
#define LOOK_AND_FEEL_PREFERENCES_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <client/client_model_types.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class LookAndFeelPreferencesWidget;
}

class QnLookAndFeelPreferencesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnLookAndFeelPreferencesWidget(QWidget *parent = 0);
    ~QnLookAndFeelPreferencesWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

    virtual bool canApplyChanges() override;
    virtual bool canDiscardChanges() override;

private:
    void setupLanguageUi();
    void setupSkinUi();
    void setupTimeModeUi();
    void setupBackgroundUi();

    void selectBackgroundImage();
private:
    QScopedPointer<Ui::LookAndFeelPreferencesWidget> ui;
    bool m_updating;

    int m_oldLanguage;
    int m_oldSkin;

    Qn::TimeMode m_oldTimeMode;

    QnClientBackground m_oldBackground;
};

#endif // LOOK_AND_FEEL_PREFERENCES_WIDGET_H
