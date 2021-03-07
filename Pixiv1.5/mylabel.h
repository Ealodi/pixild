#ifndef MYLABEL_H
#define MYLABEL_H
#include<QLabel>
#include<QString>
#include<QWidget>
#include<QMainWindow>
#include"QMessageBox"
#include<QEvent>
#include<QToolButton>
class Mylabel: public QLabel
{
    Q_OBJECT
public:
    Mylabel(QWidget *parent=0);
    ~Mylabel();
    void SetName(QString);
    void SetPage(QString);
    void Seteffect();
signals:
    void clicked();

protected:
    //void enterEvent(QEvent *);
    //void leaveEvent(QEvent *);
    void mousePressEvent(QMouseEvent* event);

private:
    QLabel *name;
    QToolButton *pic_page;
};

#endif // MYLABEL_H
