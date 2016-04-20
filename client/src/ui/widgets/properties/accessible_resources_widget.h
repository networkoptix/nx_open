#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui
{
    class AccessibleResourcesWidget;
}

class QnResourceListModel;
class QnAbstractPermissionsDelegate;

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnAccessibleResourcesWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    enum Filter
    {
        CamerasFilter,
        LayoutsFilter,
        ServersFilter
    };

    QnAccessibleResourcesWidget(QnAbstractPermissionsDelegate* delegate, Filter filter, QWidget* parent = 0);
    virtual ~QnAccessibleResourcesWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    Qn::GlobalPermission allResourcesPermission() const;

private:
    QScopedPointer<Ui::AccessibleResourcesWidget> ui;
    QnAbstractPermissionsDelegate* const m_delegate;
    const Filter m_filter;

    QScopedPointer<QnResourceListModel> m_model;
};
