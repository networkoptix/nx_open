#pragma once

//TODO: #akulikov replace with QT_VERSION_STR usage

#if QT_VERSION == 0x050201
#define QT_VERSION_NUM 5.2.1
#elif QT_VERSION == 0x050401
#define QT_VERSION_NUM 5.4.1
#elif QT_VERSION == 0x050402
#define QT_VERSION_NUM 5.4.2
#elif QT_VERSION == 0x050500
#define QT_VERSION_NUM 5.5.0
#elif QT_VERSION == 0x050501
#define QT_VERSION_NUM 5.5.1
#elif QT_VERSION == 0x050600
#define QT_VERSION_NUM 5.6.0
#else
#error "Define proper version here!"
#endif

#define QT_PRIVATE_HEADER(module, header) <module/QT_VERSION_NUM/module/private/header>

#define QT_CORE_PRIVATE_HEADER(header) QT_PRIVATE_HEADER(QtCore, header)
#define QT_GUI_PRIVATE_HEADER(header) QT_PRIVATE_HEADER(QtGui, header)
#define QT_WIDGETS_PRIVATE_HEADER(header) QT_PRIVATE_HEADER(QtWidgets, header)
#define QT_MULTIMEDIA_PRIVATE_HEADER(header) QT_PRIVATE_HEADER(QtMultimedia, header)
