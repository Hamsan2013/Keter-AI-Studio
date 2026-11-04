#include <QApplication>
#include "KeterMainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Keter AI Studio");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AD Developers");

    // Apply Dark Navy Foundation with Silver and Blue Accents
    app.setStyleSheet(
        "QMainWindow, QWidget { background-color: #0F172A; color: #E2E8F0; font-family: 'Segoe UI', -apple-system, sans-serif; }"
        "#SidebarContainer { background-color: #0B1120; border-right: 1px solid #1E293B; }"
        "#NavList { background-color: transparent; border: none; font-size: 13px; outline: none; }"
        "#NavList::item { padding: 12px 10px; border-radius: 6px; margin: 2px 4px; color: #94A3B8; }"
        "#NavList::item:selected { background-color: #1E3A8A; color: #FFFFFF; font-weight: bold; }"
        "#NavList::item:hover { background-color: #1E293B; color: #E2E8F0; }"
        "QTableWidget { background-color: #0B1120; border: 1px solid #1E293B; gridline-color: #1E293B; color: #E2E8F0; }"
        "QHeaderView::section { background-color: #1E293B; color: #93C5FD; padding: 6px; border: none; font-weight: bold; }"
        "QComboBox, QPushButton { background-color: #1E293B; color: #F8FAFC; border: 1px solid #334155; padding: 6px 12px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #334155; }"
        "QProgressBar { border: 1px solid #334155; border-radius: 6px; text-align: center; background: #0B1120; color: white; }"
        "QProgressBar::chunk { background-color: #2563EB; border-radius: 5px; }"
    );

    KeterMainWindow window;
    window.show();
    return app.exec();
}
