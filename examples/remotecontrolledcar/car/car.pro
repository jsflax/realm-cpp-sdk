QT += widgets

QMAKE_CXX=g++-10
CONFIG+=c++2a
QMAKE_CXXFLAGS+="-std=c++2a -fcoroutines -fconcepts"

HEADERS += car.h
SOURCES += car.cpp main.cpp
INCLUDEPATH += /usr/local/include/ /usr/include/

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
CONFIG += system-zlib copy-dt-needed-entries lz
# install
target.path = $$PWD
INSTALLS += target
LIBS += -L/usr/local/lib \
    -lcpprealm \
    -lrealm-object-store-dbg \
    -lrealm-sync-dbg \
    -lrealm-dbg \
    -lrealm-parser-dbg \
    -lz -lcurl
unix: LIBS += -lssl -lcrypto
macos: LIBS += -framework Foundation -framework Security
DEFINES += REALM_ENABLE_SYNC

RESOURCES += \
    resources.qrc
