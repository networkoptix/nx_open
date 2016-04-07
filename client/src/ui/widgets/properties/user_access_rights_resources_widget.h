#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserAccessRightsResourcesWidget;
}

class QnResourceListModel;

class QnUserAccessRightsResourcesWidget : public Connective<QWidget>
{
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    QnUserAccessRightsResourcesWidget(QWidget* parent = 0);
    virtual ~QnUserAccessRightsResourcesWidget();

private:
    QScopedPointer<Ui::UserAccessRightsResourcesWidget> ui;
    QScopedPointer<QnResourceListModel> m_model;
};
