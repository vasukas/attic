# qmake project file

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

CONFIG += link_pkgconfig
PKGCONFIG += glew
PKGCONFIG += glx
PKGCONFIG += x11
PKGCONFIG += xrender

SOURCES += \
	example.c \
	xglwall.c

HEADERS += \
	xglwall.h

CONFIG(debug, debug|release) {
	COMP_FLAGS = -fsanitize=address -fsanitize=undefined
	QMAKE_CXXFLAGS += $${COMP_FLAGS}
	QMAKE_LFLAGS += $${COMP_FLAGS}
}
