QtcPlugin {
    name: "AutoTest"

    Depends { name: "Core" }
    Depends { name: "CppEditor" }
    Depends { name: "CPlusPlus" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "QmlJS" }
    Depends { name: "QmlJSTools" }
    Depends { name: "Utils" }
    Depends { name: "Debugger" }
    Depends { name: "TextEditor" }

    pluginTestDepends: [
        "QbsProjectManager",
        "QmakeProjectManager"
    ]

    Depends { name: "QtSupport"; condition: qtc.withPluginTests }
    Depends { name: "Qt.testlib"; condition: qtc.withPluginTests }
    Depends { name: "Qt.widgets" }

    files: [
        "autotesticons.h",
        "autotest_global.h", "autotesttr.h",
        "autotestconstants.h",
        "autotestplugin.cpp",
        "autotestplugin.h",
        "itemdatacache.h",
        "projectsettingswidget.cpp",
        "projectsettingswidget.h",
        "testcodeparser.cpp",
        "testcodeparser.h",
        "testconfiguration.cpp",
        "testconfiguration.h",
        "testeditormark.cpp",
        "testeditormark.h",
        "testnavigationwidget.cpp",
        "testnavigationwidget.h",
        "testresult.cpp",
        "testresult.h",
        "testresultdelegate.cpp",
        "testresultdelegate.h",
        "testresultmodel.cpp",
        "testresultmodel.h",
        "testresultspane.cpp",
        "testresultspane.h",
        "testrunner.cpp",
        "testrunner.h",
        "testsettings.cpp",
        "testsettings.h",
        "testsettingspage.cpp",
        "testsettingspage.h",
        "testtreeitem.cpp",
        "testtreeitem.h",
        "testtreeitemdelegate.cpp",
        "testtreeitemdelegate.h",
        "testtreemodel.cpp",
        "testtreemodel.h",
        "testtreeview.cpp",
        "testtreeview.h",
        "testoutputreader.cpp",
        "testoutputreader.h",
        "testprojectsettings.cpp",
        "testprojectsettings.h",
        "itestparser.cpp",
        "itestparser.h",
        "itestframework.cpp",
        "itestframework.h",
        "testframeworkmanager.cpp",
        "testframeworkmanager.h",
        "testrunconfiguration.h"
    ]

    Group {
        name: "QtTest framework files"
        files: [
            "qtest/*"
        ]
    }

    Group {
        name: "Quick Test framework files"
        files: [
            "quick/*"
        ]
    }

    Group {
        name: "Google Test framework files"
        files: [
            "gtest/*"
        ]
    }

    Group {
        name: "Boost Test framework files"
        files: [
            "boost/*"
        ]
    }

    Group {
        name: "Catch framework files"
        files: [
            "catch/*"
        ]
    }

    Group {
        name: "CTest support files"
        files: [
            "ctest/*"
        ]
    }

    QtcTestFiles {
        files: [
            "autotestunittests.cpp",
            "autotestunittests.h",
            "loadprojectscenario.cpp",
            "loadprojectscenario.h",
        ]
        cpp.defines: outer.concat([ 'QTCREATORDIR="' + project.ide_source_tree + '"' ])
    }

    QtcTestResources {
        files: "unit_test/**/*"
        Qt.core.resourcePrefix: ""
    }

    Group {
        name: "images"
        files: "images/*.png"
        fileTags: "qt.core.resource_data"
    }

    Group {
        name: "Auto Test Wizard"
        prefix: "../../shared/autotest/"
        files: [
            "*"
        ]
        fileTags: []
        qbs.install: true
        qbs.installDir: qtc.ide_data_path + "/templates/wizards/autotest"
    }
}
