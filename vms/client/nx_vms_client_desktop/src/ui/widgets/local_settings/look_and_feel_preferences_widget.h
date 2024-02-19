// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <nx/vms/client/desktop/settings/types/background_image.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class LookAndFeelPreferencesWidget;
}

class QnLookAndFeelPreferencesWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnLookAndFeelPreferencesWidget(QWidget *parent = 0);
    ~QnLookAndFeelPreferencesWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;
    virtual void discardChanges() override;

    bool isRestartRequired() const;

private:
    void setupLanguageUi();
    void setupTimeModeUi();
    void setupBackgroundUi();

    void selectBackgroundImage();

    bool backgroundAllowed() const;

    QString selectedTranslation() const;
    Qn::TimeMode selectedTimeMode() const;
    Qn::ResourceInfoLevel selectedInfoLevel() const;
    int selectedTourCycleTimeMs() const;
    Qn::ImageBehavior selectedImageMode() const;
    bool isPtzAimOverlayEnabled() const;
    bool isShowTimestampOnLiveCameraEnabled() const;

private:
    QScopedPointer<Ui::LookAndFeelPreferencesWidget> ui;
    bool m_updating = false;

    nx::vms::client::desktop::BackgroundImage m_oldBackground;
};
