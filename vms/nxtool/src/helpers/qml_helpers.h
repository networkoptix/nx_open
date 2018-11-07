
#pragma once

#include <QObject>

namespace rtu
{
    namespace helpers
    {
        /// @brief Returns parent for all managed by smart pointers objects
        /// which should be transferred to Qml. This parent will be alive 
        /// for hole life of programm

        QObject *qml_objects_parent();
    }
}