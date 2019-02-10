# QT project include for Parameters UI

FORMS	+= src/parameter/ParameterWidget.ui \
           src/parameter/ParameterEntryWidget.ui
		   
HEADERS += src/parameter/ParameterWidget.h \
           src/parameter/parameterobject.h \
           src/parameter/parameterextractor.h \
           src/parameter/parametervirtualwidget.h \
           src/parameter/parameterspinbox.h \
           src/parameter/parametercombobox.h \
           src/parameter/parameterslider.h \
           src/parameter/parametercheckbox.h \
           src/parameter/parametertext.h \
           src/parameter/parametervector.h \
           src/parameter/groupwidget.h \
           src/parameter/parameterset.h

SOURCES += src/parameter/ParameterWidget.cc\
           src/parameter/parameterobject.cpp \
           src/parameter/parameterextractor.cpp \
           src/parameter/parameterspinbox.cpp \
           src/parameter/parametercombobox.cpp \
           src/parameter/parameterslider.cpp \
           src/parameter/parametercheckbox.cpp \
           src/parameter/parametertext.cpp \
           src/parameter/parametervector.cpp \
           src/parameter/groupwidget.cpp \
           src/parameter/parameterset.cpp \
           src/parameter/parametervirtualwidget.cpp
