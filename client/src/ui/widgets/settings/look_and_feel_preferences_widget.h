#ifndef LOOK_AND_FEEL_PREFERENCES_WIDGET_H
#define LOOK_AND_FEEL_PREFERENCES_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

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
    void initTranslations();

    QColor backgroundColor() const;
    void updateBackgroundColor();

    void selectBackgroundImage();

    void alignGrids();

private slots:
    void at_timeModeComboBox_activated();

private:
    QScopedPointer<Ui::LookAndFeelPreferencesWidget> ui;
    QScopedPointer<QColorDialog> m_colorDialog;
    bool m_updating;

    int m_oldLanguage;
    int m_oldSkin;

    Qn::ClientBackground m_oldBackgroundMode;
    QColor m_oldCustomBackgroundColor;

    QString m_oldBackgroundImage;
    qreal m_oldBackgroundImageOpacity;
    Qn::ImageBehaviour m_oldBackgroundImageMode;
};

#endif // LOOK_AND_FEEL_PREFERENCES_WIDGET_H
