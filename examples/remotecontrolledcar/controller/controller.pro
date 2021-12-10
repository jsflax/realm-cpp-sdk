QT += widgets network

DBUS_INTERFACES += car.xml
INCLUDEPATH += $$PWD/../
FORMS += controller.ui
HEADERS += controller.h ../car/car.h
SOURCES += main.cpp controller.cpp ../car/car.cpp
INCLUDEPATH += /usr/local/include/
CONFIG+=c++2a
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

# Work-around CI issue. Not needed in user code.
CONFIG += no_batch

# install
target.path = $$[QT_INSTALL_EXAMPLES]/dbus/remotecontrolledcar/controller
INSTALLS += target
LIBS += -L/usr/local/lib \
    -lz \
    -lcpprealm \
    -lcurl \
    -lrealm-dbg \
    -lrealm-object-store-dbg \
    -lrealm-sync-dbg \
    -lrealm-parser-dbg \
    -framework Foundation -framework Security
DEPENDPATH += $$PWD/../car/
INCLUDEPATH += $$PWD/../car/
DEFINES += REALM_ENABLE_SYNC
