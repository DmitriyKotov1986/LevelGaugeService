QT -= gui
QT += sql
QT += network

CONFIG += c++20 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../../Common/Headers

DEPENDPATH += $$PWD/../../Common/Headers

LIBS+= -L$$PWD/../../Common/Lib -lCommon

SOURCES += \
    QtService/qtservice.cpp \
    QtService/qtservice_win.cpp \
    core.cpp \
    intake.cpp \
    main.cpp \
    service.cpp \
    sync.cpp \
    tank.cpp \
    tankconfig.cpp \
    tankid.cpp \
    tanks.cpp \
    tanksconfig.cpp \
    tankstatus.cpp \
    tankstatuses.cpp \
    tconfig.cpp \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    QtService/QtService \
    QtService/QtServiceBase \
    QtService/QtServiceController \
    QtService/qtservice.h \
    QtService/qtservice_p.h \
    core.h \
    intake.h \
    service.h \
    sync.h \
    tank.h \
    tankconfig.h \
    tankid.h \
    tanks.h \
    tanksconfig.h \
    tankstatus.h \
    tankstatuses.h \
    tconfig.h

DISTFILES += \
    ReadMe.txt \

SUBDIRS += \
    QtService/service.pro

RC_ICONS = $$PWD/res/LevelGaugeService.ico


