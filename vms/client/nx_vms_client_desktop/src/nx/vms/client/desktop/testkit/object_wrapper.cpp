// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_wrapper.h"

#include <QtCore/QTimer>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>

#include <nx/vms/client/core/testkit/utils.h>

#include "utils.h"

namespace nx::vms::client::desktop::testkit {

QVariant ObjectWrapper::data(const QModelIndex& index, int role) const
{
    if (auto model = qobject_cast<QAbstractItemModel*>(m_object))
        return model->data(index, role);
    return QVariant();
}

QObject* ObjectWrapper::model()
{
    if (const auto view = qobject_cast<const QAbstractItemView*>(m_object))
        return view->model();
    return nullptr;
}

int ObjectWrapper::rowCount(const QModelIndex& parent) const
{
    if (const auto model = qobject_cast<const QAbstractItemModel*>(m_object))
        return model->rowCount(parent);
    return 0;
}
int ObjectWrapper::columnCount(const QModelIndex& parent) const
{
    if (const auto model = qobject_cast<const QAbstractItemModel*>(m_object))
        return model->columnCount(parent);
    return 0;
}

QModelIndex ObjectWrapper::index(int row, int column, const QModelIndex& parent) const
{
    if (const auto model = qobject_cast<const QAbstractItemModel*>(m_object))
        return model->index(row, column, parent);
    return QModelIndex();
}

void ObjectWrapper::setText(QString text)
{
    m_object->setProperty("text", text);
}

int ObjectWrapper::findText(QString text) const
{
    if (auto comboBox = qobject_cast<QComboBox*>(m_object))
        return comboBox->findText(text);
    return -1;
}

void ObjectWrapper::activate()
{
    if (auto action = qobject_cast<QAction*>(m_object))
    {
        QTimer::singleShot(0, m_object,
            [action]() mutable
            {
                QWindow* window = nullptr;
                const QRect rect = utils::globalRect(QVariant::fromValue(action), &window);

                if (rect.isValid() && window)
                {
                    core::testkit::utils::sendMouse(
                        rect.center(),
                        "click",
                        /* mouseButton */ Qt::LeftButton,
                        /* mouseButtons */ Qt::NoButton,
                        Qt::NoModifier,
                        window,
                        true);
                }
                else
                {
                    action->trigger();
                }
            });

    }
    else if (auto menu = qobject_cast<QMenu*>(m_object))
    {
        QTimer::singleShot(0, m_object, [menu]() mutable { menu->exec(); });
    }
}

void ObjectWrapper::activateWindow()
{
    if (auto w = qobject_cast<QWidget*>(m_object))
        w->activateWindow();
}

QVariantMap ObjectWrapper::metaInfo(QString name) const
{
    using namespace nx::vms::client::core::testkit::utils;

    QVariantMap result = getMetaInfo(m_object->metaObject(), name);
    if (!result.isEmpty())
        return result;

    return getMetaInfo(&this->staticMetaObject, name);
}

} // namespace nx::vms::client::desktop::testkit
