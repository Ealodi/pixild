#include <QLabel>
#include "mylabel.h"
#include <qdebug.h>
#include <qlabel.h>
#include <QGraphicsDropShadowEffect>
#include <QPoint>
#include <QMouseEvent>
Mylabel::Mylabel(QWidget* parent)
    :QLabel(parent)
{
    this->setParent(parent);
    this->setMouseTracking(true);
}

Mylabel::~Mylabel(){
   delete name;
   delete pic_page;
}

void Mylabel::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    emit clicked();
}

void Mylabel::SetName(QString na){
    name = new QLabel(this);
    name->setText(na);
    name->setStyleSheet("background: rgba(0,0,0,.375);font-family: 黑体;text-align: center;color: white;border-width:0;font-weight: 600;font-size: 20px;");
    name->setFixedHeight(this->height() / 3);
    name->setFixedWidth(this->width());
    name->move(0,this->height() / 3 * 2);
}

void Mylabel::SetPage(QString page){
    pic_page = new QToolButton(this);
    pic_page->setText(page);
    pic_page->setFixedWidth(40);
    pic_page->setFixedHeight(30);
    pic_page->setAttribute(Qt::WA_TranslucentBackground,true);
    pic_page->setStyleSheet("border-radius: 9px;background: rgba(0,0,0,.375);font-family: 黑体;text-align: center;color: white;border-width:0;font-weight: 600;font-size: 20px;");
    pic_page->move(this->width() - pic_page->width() - 5,5);
    pic_page->setEnabled(false);
}

void Mylabel::Seteffect(){
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect;
    effect->setOffset(4, 4);
    effect->setColor(QColor(0, 0, 0, 50));
    effect->setBlurRadius(10);
    this->setGraphicsEffect(effect);
}
