#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/common/panel.h>

namespace Ui {
class CloudUserPanelWidget;
}

class AbstractAccessor;

/** Widget for displaying cloud user heading. */
class QnCloudUserPanelWidget: public QnPanel
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    QnCloudUserPanelWidget(QWidget* parent = nullptr);
    virtual ~QnCloudUserPanelWidget();

    bool isManageLinkShown() const;
    void setManageLinkShown(bool value);

    QString email() const;
    void setEmail(const QString& value);

    QString fullName() const;
    void setFullName(const QString& value);

    static AbstractAccessor* createIconWidthAccessor();

private:
    void updateManageAccountLink();

private:
    QScopedPointer<Ui::CloudUserPanelWidget> ui;
};
