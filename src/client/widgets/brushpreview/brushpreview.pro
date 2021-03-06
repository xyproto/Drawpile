TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
INCLUDEPATH += ../../
QMAKE_CXXFLAGS += -std=c++11

# Input
RESOURCES = resources.qrc
HEADERS += ../../widgets/brushpreview.h plugin.h ../../core/layerstack.h
SOURCES += ../../core/brush.cpp ../../core/layer.cpp ../../core/layerstack.cpp ../../core/tile.cpp ../../core/rasterop.cpp
SOURCES += ../../widgets/brushpreview.cpp plugin.cpp
OTHER_FILES += brushpreview.json
