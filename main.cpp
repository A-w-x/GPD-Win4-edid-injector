#include "gpdw4edidinjector.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GpdW4EdidInjector w;
    w.show();
    return a.exec();
}
