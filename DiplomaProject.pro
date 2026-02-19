QT       += core gui widgets charts concurrent

CONFIG += c++17

# Այս տողերը շատ կարևոր են, որպեսզի include "core/Models.h"-ը աշխատի
INCLUDEPATH += $$PWD

# Ֆայլերի ցանկը
SOURCES += \
    main.cpp \
    models.cpp \
    benchmarkparser.cpp \
    annealerworker.cpp \
    costcalculator.cpp \
    mainwindow.cpp \
    designcanvas.cpp

HEADERS += \
    models.h \
    benchmarkparser.h \
    annealerworker.h \
    costcalculator.h \
    mainwindow.h \
    designcanvas.h

# Եթե օգտագործում եք Visual Studio compiler (MSVC), երբեմն պետք է սա
win32:CONFIG(release, debug|release): LIBS += -L$$[QT_INSTALL_LIBS]
