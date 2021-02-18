//============================================
//Brief:    继承QMessageBox实现自定义窗口大小
//Email:    binbin_erices@163.com
//Date:
//============================================

#ifndef AUTOMESSAGEBOX_H
#define AUTOMESSAGEBOX_H

#include <QMessageBox>
#include <QWidget>
#include <QResizeEvent>

class CAutoMessageBox :public QMessageBox
{
    Q_OBJECT
public:
    explicit CAutoMessageBox(QWidget* parent = 0);
    ~CAutoMessageBox() {}

public:
    void AutoSetSize(int width, int high);

protected:
    void resizeEvent(QResizeEvent* event);

private:
    int m_width;
    int m_high;

};


#endif // AUTOMESSAGEBOX_H
