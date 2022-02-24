// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/panel.h>

#include <nx/vms/client/desktop/common/utils/filesystem.h>

namespace Ui {
class FilenamePanel;
}

namespace nx::vms::client::desktop {

class FilenamePanel: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit FilenamePanel(QWidget *parent = 0);
    virtual ~FilenamePanel() override;

    FileExtensionList allowedExtesions() const;
    void setAllowedExtensions(const FileExtensionList& extensions);

    Filename filename() const;
    void setFilename(const Filename& value);

    bool validate();

signals:
    bool filenameChanged(const Filename& filename);

private:
    void updateExtension();

private:
    struct Private;
    QScopedPointer<Ui::FilenamePanel> ui;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
