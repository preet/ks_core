INCLUDEPATH += $${PWD}

# ks
PATH_KS_CORE = $${PWD}/ks

# core
HEADERS += \
    $${PATH_KS_CORE}/KsConfig.hpp \
    $${PATH_KS_CORE}/KsGlobal.hpp \
    $${PATH_KS_CORE}/KsLog.hpp \
    $${PATH_KS_CORE}/KsException.hpp \
    $${PATH_KS_CORE}/KsMiscUtils.hpp \
    $${PATH_KS_CORE}/KsEvent.hpp \
    $${PATH_KS_CORE}/KsTask.hpp \
    $${PATH_KS_CORE}/KsEventLoop.hpp \
    $${PATH_KS_CORE}/KsObject.hpp \
    $${PATH_KS_CORE}/KsSignal.hpp \
    $${PATH_KS_CORE}/KsTimer.hpp

SOURCES += \
    $${PATH_KS_CORE}/KsLog.cpp \
    $${PATH_KS_CORE}/KsException.cpp \
    $${PATH_KS_CORE}/KsTask.cpp \
    $${PATH_KS_CORE}/KsEventLoop.cpp \
    $${PATH_KS_CORE}/KsObject.cpp \
    $${PATH_KS_CORE}/KsSignal.cpp \
    $${PATH_KS_CORE}/KsTimer.cpp

# thirdparty
include($${PATH_KS_CORE}/thirdparty/asio/asio.pri)

# need to link pthreads to use std::thread
!android {
    LIBS += -lpthread
}

QMAKE_CXXFLAGS += -std=c++11

