#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui {
class GeneralPreferencesWidget;
}

class QnVideoRecorderSettings;

class QnGeneralPreferencesWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit QnGeneralPreferencesWidget(QnVideoRecorderSettings* settings, QWidget *parent = 0);
    ~QnGeneralPreferencesWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

signals:
    void recordingSettingsChanged();
    void mediaDirectoriesChanged();

private:
    void at_addMediaFolderButton_clicked();
    void at_removeMediaFolderButton_clicked();
    void at_mediaFoldersList_selectionChanged();

private:
    QStringList mediaFolders() const;
    void setMediaFolders(const QStringList& value);

    quint64 userIdleTimeoutMs() const;
    void setUserIdleTimeoutMs(quint64 value);

    bool autoStart() const;
    void setAutoStart(bool value);

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString &name);

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString &name);
private:
    QScopedPointer<Ui::GeneralPreferencesWidget> ui;
    QnVideoRecorderSettings* m_recorderSettings;
};
