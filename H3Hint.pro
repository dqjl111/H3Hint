QT += widgets

CONFIG += c++17
CONFIG -= app_bundle
CONFIG -= debug_and_release
CONFIG += release

TARGET = H3Hint
TEMPLATE = app

# 源文件以 UTF-8 保存，告知 MinGW 源编码，避免中文乱码
win32 {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8
}

SOURCES += \
    src/main.cpp \
    src/project.cpp \
    src/generator.cpp \
    src/mainwindow.cpp \
    src/appconfigdialog.cpp \
    src/gendialog.cpp \
    src/previewdock.cpp \
    src/theme.cpp

HEADERS += \
    src/project.h \
    src/generator.h \
    src/mainwindow.h \
    src/appconfigdialog.h \
    src/gendialog.h \
    src/previewdock.h \
    src/theme.h

# 图标：EXE 资源 + 窗口/任务栏图标
RESOURCES += resources/appicon.qrc
win32 {
    RC_FILE = resources/appicon.rc
}
