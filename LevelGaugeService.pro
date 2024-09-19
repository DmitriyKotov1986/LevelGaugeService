QT -= gui
QT += sql
QT += network

CONFIG += c++20 console
CONFIG -= app_bundle

SOURCES += \
    core.cpp \
    intake.cpp \
    main.cpp \
    service.cpp \
    suncsync.cpp \
    sync.cpp \
    syncdbintake.cpp \
    syncdbstatus.cpp \
    synchttpintake.cpp \
    synchttpstatus.cpp \
    tank.cpp \
    tankconfig.cpp \
    tankid.cpp \
    tanks.cpp \
    tanksconfig.cpp \
    tankstatus.cpp \
    tankstatuses.cpp \
    tconfig.cpp \

HEADERS += \
    core.h \
    intake.h \
    service.h \
    suncsync.h \
    sync.h \
    syncdbintake.h \
    syncdbstatus.h \
    synchttpintake.h \
    synchttpstatus.h \
    tank.h \
    tankconfig.h \
    tankid.h \
    tanks.h \
    tanksconfig.h \
    tankstatus.h \
    tankstatuses.h \
    tconfig.h

RC_ICONS = $$PWD/res/LevelGaugeService.ico

include($$PWD/../../Common/Common/Common.pri)
include($$PWD/../../QtService/QtService/QtService.pri)


