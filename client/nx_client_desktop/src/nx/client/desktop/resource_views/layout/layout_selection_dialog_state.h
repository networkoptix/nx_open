#pragma once

#include <QtCore/QHash>

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/layout/ordered_list.h>

namespace nx {
namespace client {
namespace desktop {


struct LayoutSelectionDialogState
{
    using LayoutId = QnUuid;
    using SubjectId = QnUuid;

    struct LayoutData
    {
        QString name;
        bool shared = false;
        bool checked = false;
    };
    using Layouts = OrderedList<LayoutId, LayoutData>;

    using SubjectName = QString;
    using Subjects = OrderedList<SubjectId, SubjectName>;

    using LayoutIdList = QList<LayoutId>;
    using Relations = QHash<SubjectId, LayoutIdList>;

    bool flatView = false;
    Layouts layouts;
    Subjects subjects;
    Relations relations;
};

} // namespace desktop
} // namespace client
} // namespace nx
