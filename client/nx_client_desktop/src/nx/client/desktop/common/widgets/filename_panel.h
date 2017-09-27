#pragma once

#include <ui/widgets/common/panel.h>

#include <nx/client/desktop/common/utils/filesystem.h>

namespace Ui {
class FilenamePanel;
}

namespace nx {
namespace client {
namespace desktop {

class FilenamePanel: public QnPanel
{
    Q_OBJECT
    using base_type = QnPanel;

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

} // namespace desktop
} // namespace client
} // namespace nx
