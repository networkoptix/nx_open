#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/layout/base_store.h>
#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_state.h>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {

class LayoutSelectionDialogState;

class LayoutSelectionDialogStore: public BaseStore<LayoutSelectionDialogState>
{
    Q_OBJECT
    using base_type = BaseStore<LayoutSelectionDialogState>;

public:
    LayoutSelectionDialogStore(QObject* parent = nullptr);

    // Actions.
    void setLayoutSelection(const QnUuid& layoutId, bool selected);
    void setSubjectSelection(const QnUuid& subjectId, bool selected);

    void setResources(const QnResourceList& resources);
};

} // namespace desktop
} // namespace client
} // namespace nx

