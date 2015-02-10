INCLUDEPATH += $${PWD}

# ks
PATH_KS_CORE = $${PWD}/ks

# core
HEADERS += \
    $${PATH_KS_CORE}/KsGlobal.h \
    $${PATH_KS_CORE}/KsLog.h \
    $${PATH_KS_CORE}/KsMiscUtils.h \
    $${PATH_KS_CORE}/KsEvent.h \
    $${PATH_KS_CORE}/KsEventLoop.h \
    $${PATH_KS_CORE}/KsObject.h \
    $${PATH_KS_CORE}/KsSignal.h \
    $${PATH_KS_CORE}/KsTimer.h \
    $${PATH_KS_CORE}/KsApplication.h

SOURCES += \
    $${PATH_KS_CORE}/KsLog.cpp \
    $${PATH_KS_CORE}/KsEventLoop.cpp \
    $${PATH_KS_CORE}/KsObject.cpp \
    $${PATH_KS_CORE}/KsTimer.cpp \
    $${PATH_KS_CORE}/KsApplication.cpp

# thirdparty
include($${PATH_KS_CORE}/thirdparty/asio/asio.pri)

# need these flags for gcc 4.8.x bug for threads
QMAKE_LFLAGS += -Wl,--no-as-needed
LIBS += -lpthread
QMAKE_CXXFLAGS += -std=c++11

