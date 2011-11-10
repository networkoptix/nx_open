#include "archivedropinstrument.h"
#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>
#include <plugins/resources/archive/avi_files/avi_dvd_device.h>
#include <plugins/resources/archive/avi_files/avi_bluray_device.h>
#include <plugins/resources/archive/avi_files/avi_dvd_archive_delegate.h>
#include <plugins/resources/archive/filetypesupport.h>
#include <ui/scene/display_state.h>
#include <ui/scene/display_grid_mapper.h>
#include <ui/model/display_model.h>
#include <ui/model/display_entity.h>
#include <utils/common/warnings.h>

ArchiveDropInstrument::ArchiveDropInstrument(QnDisplayState *state, QObject *parent):
    Instrument(makeSet(/*QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove, QEvent::GraphicsSceneDrop*/), makeSet(/*QEvent::DragEnter, QEvent::Drop*/), makeSet(QEvent::DragEnter, QEvent::DragMove, QEvent::DragLeave, QEvent::Drop), makeSet(), parent),
    m_state(state)
{
    if(state == NULL) {
        qnNullWarning(state);
        return;
    }
}

bool ArchiveDropInstrument::dragEnterEvent(QWidget *viewport, QDragEnterEvent *event) {
    if(m_state == NULL)
        return false;

    m_files.clear();
    foreach (const QUrl &url, event->mimeData()->urls())
        findAcceptedFiles(url.toLocalFile());
    if (m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragMoveEvent(QWidget *viewport, QDragMoveEvent *event) {
    if(m_state == NULL)
        return false;

    if(m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragLeaveEvent(QWidget *viewport, QDragLeaveEvent *event) {
    if(m_state == NULL)
        return false;

    if(m_files.empty())
        return false;

    return true;
}

bool ArchiveDropInstrument::dropEvent(QWidget *viewport, QDropEvent *event) {
    if(m_state == NULL)
        return false;

    QnResourceList resources = QnResourceDirectoryBrowser::instance().checkFiles(m_files);
    qnResPool->addResources(resources);

    QRect geometry(m_state->gridMapper()->mapToGrid(view(viewport)->mapToScene(event->pos())), QSize(1, 1));
    foreach(QnResourcePtr resource, resources) {
        QnDisplayEntity *entity = new QnDisplayEntity(resource);
        entity->setGeometry(geometry);

        m_state->model()->addEntity(entity);
        if(!entity->isPinned()) {
            /* Place already taken, pick closest one. */
            QRect newGeometry = m_state->model()->closestFreeSlot(geometry.topLeft(), geometry.size());
            m_state->model()->pinEntity(entity, newGeometry);
        }
    }

    return true;
}

void ArchiveDropInstrument::findAcceptedFiles(const QString &path) {
    if (CLAviDvdDevice::isAcceptedUrl(path)) {
        if (path.indexOf(QLatin1Char('?')) == -1) {
            /* Open all titles on a DVD. */
            QStringList titles = QnAVIDvdArchiveDelegate::getTitleList(path);
            foreach (const QString &title, titles)
                m_files.push_back(path + QLatin1String("?title=") + title);
        } else {
            m_files.push_back(path);
        }
    } else if (CLAviBluRayDevice::isAcceptedUrl(path)) {
        m_files.push_back(path);
    } else {
        FileTypeSupport fileTypeSupport;
        QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            QDirIterator i(path, QDirIterator::Subdirectories);
            while (i.hasNext()) {
                QString nextFilename = i.next();
                if (QFileInfo(nextFilename).isFile()) {
                    if (fileTypeSupport.isFileSupported(nextFilename))
                        m_files.push_back(nextFilename);
                }
            }
        } else if (fileInfo.isFile()) {
            if (fileTypeSupport.isFileSupported(path))
                m_files.push_back(path);
        }
    }
}
