#include "pixiv.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Pixiv w;
    w.show();
    return a.exec();
}
