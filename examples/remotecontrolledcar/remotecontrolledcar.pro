TEMPLATE = subdirs
SUBDIRS = car \
          controller

unix|win32: LIBS += -lrealm -lrealm-object-store -lcpprealm
INCLUDEPATH += /usr/local/include/
DEFINES += REALM_ENABLE_SYNC
