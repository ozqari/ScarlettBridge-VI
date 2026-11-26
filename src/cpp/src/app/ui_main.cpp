// scarlettbridge-ui entry: Qt Widgets control center shell.
#include <QApplication>
#include "ui/ui_shell.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  UiShell shell;
  shell.show();
  return app.exec();
}
