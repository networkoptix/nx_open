#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserAccessRightsWidget;
}

class QnUserAccessRightsWidget : public Connective<QWidget>
{
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    QnUserAccessRightsWidget(QWidget* parent = 0);
    virtual ~QnUserAccessRightsWidget();

private:
    QScopedPointer<Ui::UserAccessRightsWidget> ui;
};
