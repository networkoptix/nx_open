#pragma once

#include <ui/widgets/common/panel.h>

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

    QStringList allowedExtesions() const;
    void setAllowedExtensions(const QStringList& extensions);

    QString filename() const;
    void setFilename(const QString& value);

signals:
    bool filenameChanged(const QString& filename);

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
