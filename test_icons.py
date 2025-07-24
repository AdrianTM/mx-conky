import sys
from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QIcon

app = QApplication(sys.argv)

# Test various icon names
icons_to_test = [
    "preferences-other-symbolic",
    "preferences-system-symbolic", 
    "document-edit-symbolic",
    "edit-symbolic",
    "configure-symbolic",
    "settings-symbolic",
    "customize-symbolic",
    "tools-symbolic",
    "wrench-symbolic",
    "gear-symbolic",
    "document-properties-symbolic",
    "system-run-symbolic"
]

for icon_name in icons_to_test:
    icon = QIcon.fromTheme(icon_name)
    if not icon.isNull():
        print(f"✓ {icon_name}")
    else:
        print(f"✗ {icon_name}")

