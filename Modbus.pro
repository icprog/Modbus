TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += main.cpp \
    libmodbus/modbus-tcp.c \
    libmodbus/modbus-rtu.c \
    libmodbus/modbus-data.c \
    libmodbus/modbus.c

HEADERS += \
    libmodbus/modbus-version.h \
    libmodbus/modbus-tcp-private.h \
    libmodbus/modbus-tcp.h \
    libmodbus/modbus-rtu-private.h \
    libmodbus/modbus-rtu.h \
    libmodbus/modbus-private.h \
    libmodbus/modbus.h \
    libmodbus/win-defs/stdint.h \
    libmodbus/win-defs/inttypes.h
win32{
    INCLUDEPATH += E:\boost_1_49_0

    CONFIG(debug,debug|release){
        LIBS +=E:\boost_1_49_0\stage\lib\libboost_thread-vc100-mt-gd-1_49.lib
    }else{
       LIBS +=E:\boost_1_49_0\stage\lib\libboost_thread-vc100-mt-1_49.lib
    }
}
unix{
    INCLUDEPATH += /home/lihaibo/dev/boost_1_49_0
    LIBS += /home/lihaibo/dev/boost_1_49_0/stage/lib/libboost_thread.a
    LIBS += -lpthread
}
INCLUDEPATH +=libmodbus
win32:LIBS +=ws2_32.lib
win32:LIBS +=wsock32.lib
win32:LIBS +=setupapi.lib