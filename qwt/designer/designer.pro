################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

QWT_ROOT = ..

include ( $${QWT_ROOT}/qwtconfig.pri )

contains(CONFIG, QwtDesigner) {

	CONFIG    += qt designer plugin 
	CONFIG    += warn_on

	TEMPLATE        = lib
	TARGET          = qwt_designer_plugin

	DESTDIR         = plugins/designer

	INCLUDEPATH    += $${QWT_ROOT}/src 
	DEPENDPATH     += $${QWT_ROOT}/src 

	LIBS      += -L$${QWT_ROOT}/lib 
	qtAddLibrary(qwt)

	contains(CONFIG, QwtDll) {
		win32 {
			DEFINES += QT_DLL QWT_DLL
		}
	}

	!contains(CONFIG, QwtPlot) {
		DEFINES += NO_QWT_PLOT
	}

	!contains(CONFIG, QwtWidgets) {
		DEFINES += NO_QWT_WIDGETS
	}

	HEADERS += qwt_designer_plugin.h
	SOURCES += qwt_designer_plugin.cpp

	contains(CONFIG, QwtPlot) {

		HEADERS += qwt_designer_plotdialog.h
		SOURCES += qwt_designer_plotdialog.cpp
	}

	RESOURCES += qwt_designer_plugin.qrc

	target.path = $${QWT_INSTALL_PLUGINS}
	INSTALLS += target
}
else {
	TEMPLATE        = subdirs # do nothing
}
