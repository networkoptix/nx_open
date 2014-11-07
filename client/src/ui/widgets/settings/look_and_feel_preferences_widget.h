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

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;

    virtual bool confirm() override;
    virtual bool discard() override;

protected:
    virtual bool event(QEvent *event) override;

private:
    void setupLanguageUi();
    void setupSkinUi();
    void setupTimeModeUi();
    void setupBackgroundUi();

    QColor animationCustomColor() const;
    void updateAnimationCustomColor();

    void selectBackgroundImage();
private:
    QScopedPointer<Ui::LookAndFeelPreferencesWidget> ui;
    QScopedPointer<QColorDialog> m_colorDialog;
    bool m_updating;

    int m_oldLanguage;
    int m_oldSkin;

    Qn::TimeMode m_oldTimeMode;

    QnClientBackground m_oldBackground;
};

#endif // LOOK_AND_FEEL_PREFERENCES_WIDGET_H
