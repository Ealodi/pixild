#include "automessagebox.h"

CAutoMessageBox::CAutoMessageBox(QWidget* parent) :QMessageBox(parent)
, m_width(0)
, m_high(0)
{
    this->AutoSetSize(320, 160);
    this->setStyleSheet("font:18px;font-family:'微软雅黑Light';font-weight:bold;");
}

void CAutoMessageBox::AutoSetSize(int width, int high)
{
    m_width = width;
    m_high = high;
}

void CAutoMessageBox::resizeEvent(QResizeEvent* event)
{
    setFixedSize(m_width, m_high);
}

