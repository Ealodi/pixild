#ifndef PIXIV_H
#define PIXIV_H

#include <QMainWindow>
#include <QtNetwork>
#include <vector>
#include "automessagebox.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Pixiv; }
QT_END_NAMESPACE


class Pixiv : public QMainWindow
{
    Q_OBJECT

public:
    QPixmap Radius(QPixmap pic,int width,int height);
    void print(QString info);
    QNetworkRequest *request;
    QNetworkAccessManager *m_accessManager;
    CAutoMessageBox* msg = new CAutoMessageBox;
    Pixiv(QWidget *parent = nullptr);
    ~Pixiv();
signals:
    void finished(QNetworkReply*);//发送接收信号
    void Requestover(QByteArray*);
    void DownOriginal(QString);
    void StartSearchPic(QString id);//开始爬取插画
    void StartSearchArt(QString id);//开始爬取画师
    void StartSearchRank(QString mode,QString time);//开始爬取排行榜
    void StartSearchTag(QString word);//开始爬取搜索内容
private slots:
    void TagSearch(QString word);//搜索内容
    void DealRank(QByteArray*);//处理排行榜数据
    void rank(QString mode,QString time);//排行榜
    void DealArticlePic(QByteArray*);//获取画师的插画
    void finishedSlot(QNetworkReply* reply);//接收数据
    void DealSearchContent(QByteArray* content);//解析接收到的数据(插画)
    void DealAritcleContent(QByteArray* content);//解析接收到的数据(画师)
    void DealRadomPic(QByteArray* content);//处理随机图片
    void PicClicked(QString id);//爬取插画
    void ArticleClicked(QString id);//画师ID
    void on_toolButton_clicked();
    void on_toolButton_2_clicked();
    void on_toolButton_3_clicked();
    void on_toolButton_4_clicked();
    void on_liulan_clicked();
    void on_textEdit_textChanged();
    void on_pushButton_2_clicked();

private:
    Ui::Pixiv *ui;
};

QString ChanHost(QString url,QString hosts);//换HOST
void ReadSetting();//读取设置
QString GetTime();//获取当前时间

class Setting{
public:
    QString path = "./";
    bool R18 = false;
    int PicQuality = 0;
    bool rankAuto = false;
};
//图片类
class ImgInfo{
private:
    unsigned int status_code = 0;
    unsigned int page_count = 0;
    unsigned long PicID = 0;
    unsigned long UserID = 0;
    QString UserName = "",PicName = "";
public:
    ImgInfo(){
        status_code = 0;page_count = 0;PicID = 0;UserID = 0;QString UserName = "";
        PicName = "";
    }
    void clear(){
        status_code = 0;page_count = 0;PicID = 0;UserID = 0;QString UserName = "";
        PicName = "";
        original.clear();
        square_medium.clear();
    }
    ImgInfo(unsigned status_code_,
            unsigned int page_count_,
            unsigned long PicID_,
            unsigned long UserID_,

            QString UserName_,
            QString PicName_
            ){
        status_code = status_code_;
        page_count = page_count_;
        PicID = PicID_;
        UserID = UserID_;

        UserName = UserName_;
        PicName = PicName_;
    }
    std::vector<QString>square_medium;
    std::vector<QString>original;
    void PushAUrlToMedium(QString url){
        square_medium.push_back(url);
    }
    void PushAUrlToOrigin(QString url){
        original.push_back(url);
    }
    unsigned int GetStatus_code() const{
        return status_code;
    }
    unsigned int GetPage_count() const{
        return page_count;
    }
    unsigned long GetPicID() const{
        return PicID;
    }
    unsigned long GetUserID() const{
        return UserID;
    }
    QString GetUserName() const{
        return UserName;
    }
    QString GetPicName() const{
        return PicName;
    }

    void SetStatus_code(unsigned status_code_){
        status_code = status_code_;
    }
    void SetPage_count(unsigned Page_count_){
        page_count = Page_count_;
    }
    void SetPicID(unsigned long PicID_){
        PicID = PicID_;
    }
    void SetUserID(unsigned long UserID_){
        UserID = UserID_;
    }
    void SetUserName(QString UserName_){
        UserName = UserName_;
    }
    void SetPicName(QString PicName_){
        PicName = PicName_;
    }
};

#endif // PIXIV_H
