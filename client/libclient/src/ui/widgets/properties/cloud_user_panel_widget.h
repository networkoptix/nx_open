#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/common/panel.h>

namespace Ui {
class CloudUserPanelWidget;
}

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnCloudUserPanelWidget : public QnPanel
{
    Q_OBJECT

    typedef QnPanel base_type;
public:
    QnCloudUserPanelWidget(QWidget* parent = 0);
    virtual ~QnCloudUserPanelWidget();

    bool enabled() const;
    void setEnabled(bool value);

    bool showEnableButton() const;
    void setShowEnableButton(bool value);

    bool showLink() const;
    void setShowLink(bool value);

    QString email() const;
    void setEmail(const QString& value);

    QString fullName() const;
    void setFullName(const QString& value);

signals:
    void enabledChanged(bool value);

private:
    QScopedPointer<Ui::CloudUserPanelWidget> ui;
};
