import sys
from PyQt6.QtWidgets import QApplication
from asset_tracker.config import APP_NAME, APP_VERSION
from asset_tracker.database import Database
from asset_tracker.ui.main_window import MainWindow


def main() -> None:
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)
    app.setStyle("Fusion")

    db = Database()
    window = MainWindow(db)
    window.show()

    exit_code = app.exec()
    db.close()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
