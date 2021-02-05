INCLUDEPATH += .

HEADERS += \
src/doc_list.h \
src/doc_list_model.h \
src/label_list.h \
src/label_list_model.h \
src/dataset_menu.h \
src/user_roles.h \
src/main_window.h \
src/annotator.h \
src/searchable_text.h \
src/annotations_model.h \
src/no_deselect_all_view.h \
src/database.h \
src/import_export_menu.h \
src/utils.h \


SOURCES += \
src/main.cpp \
src/doc_list.cpp \
src/doc_list_model.cpp \
src/label_list.cpp \
src/label_list_model.cpp \
src/dataset_menu.cpp \
src/main_window.cpp \
src/annotator.cpp \
src/searchable_text.cpp \
src/annotations_model.cpp \
src/no_deselect_all_view.cpp \
src/database.cpp \
src/import_export_menu.cpp \
src/utils.cpp \

QT += widgets sql
RESOURCES = resources.qrc

test {
TEMPLATE = app
TARGET = labelbuddy_tests

QT += testlib

INCLUDEPATH += ../src/

RESOURCES += test_resources.qrc

HEADERS += \
test/testing_utils.h \
test/test_searchable_text.h \
test/test_database.h \
test/test_doc_list_model.h \
test/test_label_list_model.h \
test/test_annotations_model.h \
test/test_annotator.h \

SOURCES += \
test/main.cpp \
test/testing_utils.cpp \
test/test_searchable_text.cpp \
test/test_database.cpp \
test/test_doc_list_model.cpp \
test/test_label_list_model.cpp \
test/test_annotations_model.cpp \
test/test_annotator.cpp \

SOURCES -= src/main.cpp
}

else {
TEMPLATE = app
TARGET = labelbuddy
}
