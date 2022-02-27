// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

/**
 * Instance of this class is accessible from the workbench context and holds Resource Tree
 * visual state and display settings. Data is kept in sync with Client State storage, thus
 * have local scope and may be different for each user. It's not applicable for system wide
 * settings.
 * @note Right place to store data regarding nodes in expanded state and other view-related data.
 */
class NX_VMS_CLIENT_DESKTOP_API ResourceTreeSettings: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showServersInTree READ showServersInTree WRITE setShowServersInTree NOTIFY showServersInTreeChanged)
    using base_type = QObject;
    class StateDelegate;

public:
    ResourceTreeSettings(QObject* parent = nullptr);
    virtual ~ResourceTreeSettings() override;

    /**
     * Setting which suggests whether devices displayed in the Resource Tree will be grouped
     * by parent server or will be displayed all together within 'Cameras & Devices' group.
     * For non-administrator users actual display mode is defined by combination of this
     * setting and <tt>showServersInTreeForNonAdmins</tt> global setting. Please note that
     * besides main Resource Tree this setting affect device selection dialogs either.
     */
    bool showServersInTree() const;
    void setShowServersInTree(bool show);

signals:
    void showServersInTreeChanged();

private:
    void clear();

private:
    bool m_showServersInResourceTree = true;
};

} // namespace nx::vms::client::desktop
