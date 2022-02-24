// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef VIDEOWALL_SETTINGS_DIALOG_H
#define VIDEOWALL_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui {
class QnVideowallSettingsDialog;
}

class QnVideowallSettingsDialog: public QnButtonBoxDialog
{
    Q_OBJECT

    using base_type = QnButtonBoxDialog;
public:
    explicit QnVideowallSettingsDialog(QWidget* parent);
    ~QnVideowallSettingsDialog();

    void loadFromResource(const QnVideoWallResourcePtr& videowall);
    void submitToResource(const QnVideoWallResourcePtr& videowall);

private:
    QScopedPointer<Ui::QnVideowallSettingsDialog> ui;
};

#endif // VIDEOWALL_SETTINGS_DIALOG_H
