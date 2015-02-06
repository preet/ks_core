TEMPLATE    = app
TARGET      = ks
CONFIG      -= qt

INCLUDEPATH += $${_PRO_FILE_PWD_}

# ks
PATH_KS = $${_PRO_FILE_PWD_}/ks

# core
HEADERS += \
    $${PATH_KS}/KsGlobal.h \
    $${PATH_KS}/KsLog.h \
    $${PATH_KS}/KsEvent.h \
    $${PATH_KS}/KsEventLoop.h \
    $${PATH_KS}/KsObject.h \
    $${PATH_KS}/KsSignal.h \
    $${PATH_KS}/KsTimer.h \
    $${PATH_KS}/KsApplication.h

SOURCES += \
    $${PATH_KS}/KsLog.cpp \
    $${PATH_KS}/KsEventLoop.cpp \
    $${PATH_KS}/KsObject.cpp \
    $${PATH_KS}/KsTimer.cpp \
    $${PATH_KS}/KsApplication.cpp

# test
SOURCES += \
    $${PATH_KS}/test/KsTest.cpp

# thirdparty
include($${PATH_KS}/thirdparty/asio/asio.pri)

# need these flags for gcc 4.8.x bug for threads
QMAKE_LFLAGS += -Wl,--no-as-needed
LIBS += -lpthread
QMAKE_CXXFLAGS += -std=c++11

