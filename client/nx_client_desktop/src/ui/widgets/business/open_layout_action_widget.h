#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui {
class OpenLayoutActionWidget;
} // namespace Ui

class QnResourceListModel;

namespace nx {
namespace client {
namespace desktop {

/**
 * A widget to edit parameters for openLayoutAction
 * Provides UI to:
 *  - set layout to be opened
 *  - set user/role list
 */
class OpenLayoutActionWidget : public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit OpenLayoutActionWidget(QWidget* parent = nullptr);
    virtual ~OpenLayoutActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
    void at_layoutSelectionChanged(int index);

private:
    QScopedPointer<Ui::OpenLayoutActionWidget> ui;

    QnResourceListModel* m_listModel;
};

} // namespace desktop
} // namespace client
} // namespace nx
