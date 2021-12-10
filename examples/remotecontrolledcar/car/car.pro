QT += widgets network

DBUS_ADAPTORS += car.xml
HEADERS += car.h
SOURCES += car.cpp main.cpp
INCLUDEPATH += /usr/local/include/
CONFIG+=c++2a
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
# install
target.path = $$[QT_INSTALL_EXAMPLES]/dbus/remotecontrolledcar/car
INSTALLS += target
LIBS += -L/usr/local/lib \
    -lz \
    -lcurl \
    -lcpprealm \
    -lrealm-dbg \
    -lrealm-object-store-dbg \
    -lrealm-sync-dbg \
    -lrealm-parser-dbg \
    -framework Foundation -framework Security
DEFINES += REALM_ENABLE_SYNC

RESOURCES += \
    resources.qrc
