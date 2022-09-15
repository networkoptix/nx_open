// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui { class PlaySoundBusinessActionWidget; }
class QnWorkbenchContext;

namespace nx::vms::client::desktop { class ServerNotificationCache; }

class QnPlaySoundBusinessActionWidget:
    public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit QnPlaySoundBusinessActionWidget(
        QnWorkbenchContext* context,
        QWidget* parent = nullptr);
    virtual ~QnPlaySoundBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();
    void updateCurrentIndex();
    void enableTestButton();

    void at_testButton_clicked();
    void at_manageButton_clicked();
    void at_soundModel_itemChanged(const QString& filename);
    void at_volumeSlider_valueChanged(int value);

private:
    QScopedPointer<Ui::PlaySoundBusinessActionWidget> ui;
    QString m_filename;
    nx::vms::client::desktop::ServerNotificationCache* const m_serverNotificationCache;
};
