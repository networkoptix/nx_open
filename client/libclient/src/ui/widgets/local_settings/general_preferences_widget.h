#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui {
class GeneralPreferencesWidget;
}

class QnGeneralPreferencesWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit QnGeneralPreferencesWidget(QWidget *parent = 0);
    ~QnGeneralPreferencesWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

private slots:
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();

private:
    QString mainMediaFolder() const;
    void setMainMediaFolder(const QString& value);

    QStringList extraMediaFolders() const;
    void setExtraMediaFolders(const QStringList& value);

    quint64 userIdleTimeoutMs() const;
    void setUserIdleTimeoutMs(quint64 value);

    bool autoStart() const;
    void setAutoStart(bool value);
private:
    QScopedPointer<Ui::GeneralPreferencesWidget> ui;
};
