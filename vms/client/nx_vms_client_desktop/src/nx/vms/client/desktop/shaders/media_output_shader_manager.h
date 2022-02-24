// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "media_output_shader_program.h"

class QOpenGLContext;

namespace nx::vms::client::desktop {

class MediaOutputShaderManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static MediaOutputShaderManager* instance(QOpenGLContext* context);

    MediaOutputShaderProgram* program(const MediaOutputShaderProgram::Key& key);

    QOpenGLContext* context() const;

protected:
    explicit MediaOutputShaderManager(QOpenGLContext* context);
    virtual ~MediaOutputShaderManager() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
