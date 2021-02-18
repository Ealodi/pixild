#include "pixiv.h"
#include "ui_pixiv.h"
#include <qdebug.h>
#include <QtNetwork>
#include <QScrollArea>
#include <thread>
#include <qlabel.h>
#include <vector>
#include <QFileDialog>
#include "mylabel.h"
#include <QDesktopServices>
#include <QPainter>

#define FIRST_HOST "210.140.92.138"

typedef std::vector<Mylabel*> Display;
typedef const QString SOFTWARE_VERSION;
/*全局变量*/
SOFTWARE_VERSION VERSION = "1.2";
ImgInfo m_imgs;//图片信息
std::vector<QLabel*>m_Piclabel;//上一次显示的图片label指针
std::vector<QLabel*>m_Articlelabel;//上一次显示的作家label指针
QLabel* m_AHead;//画师头像
Setting m_set;//设置信息
int m_ArtY = 0;//画师插画的下一个y坐标
int m_RankY = 50;//排行榜插画下一个y坐标
int m_TagY = 50;//tag榜下一个Y
int m_page_pic = 1;//画师插画页数
int m_page_tag = 1;//tag搜索页码
QString m_next_url_rank = "";//排行榜下一页url
Display m_Pics;//画师插画
Display m_rankPic;//排行榜插画
Display m_tagPic;//tag插画
/*全局变量*/

Pixiv::Pixiv(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Pixiv)
{
    ui->setupUi(this);
    ui->scrollArea->setFrameShape(QFrame::NoFrame);
    ui->scrollArea_2->setFrameShape(QFrame::NoFrame);
    ui->scrollArea_3->setFrameShape(QFrame::NoFrame);
    ui->scrollArea_4->setFrameShape(QFrame::NoFrame);
    ui->scrollArea_5->setFrameShape(QFrame::NoFrame);

    ui->pushButton_3->setVisible(false);
    ui->pushButton_4->setVisible(false);
    //设置https
    request = new QNetworkRequest;
    m_accessManager = new QNetworkAccessManager();

    QSslConfiguration config;
    QSslConfiguration conf = request->sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    conf.setProtocol(QSsl::TlsV1SslV3);
    request->setSslConfiguration(conf);
    connect(m_accessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finishedSlot(QNetworkReply*)));
    //判断最新版本
    QNetworkAccessManager manager;
    QEventLoop loop;
    request->setUrl(QUrl("https://gitee.com/ailou/pixiv/raw/master/version.txt"));
    QNetworkReply *reply = manager.get(*request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray content = reply->readAll();

    QString version(content);
    if (version != VERSION)
    {
        if (QMessageBox::Yes == QMessageBox::information(NULL, "提示", "网络未连接或者已有新版本，请前往https://ealodi.xyz查看。",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)){
            QDesktopServices::openUrl(QString("https://ealodi.xyz"));
        }
        this->close();
        this->deleteLater();
    }

    //读取设置内容
    ReadSetting();
    ui->lineEdit_2->setText(m_set.path);
    ui->checkBox->setChecked(m_set.R18);
    ui->checkBox_2->setChecked(m_set.rankAuto);
    ui->comboBox_2->setCurrentIndex(m_set.PicQuality);
    //信息框
    msg->setWindowTitle("提示:");
    msg->setIcon(QMessageBox::Question);
    ui->textEdit_2->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->pushButton->setVisible(false);
    //连接信号
    connect(this,SIGNAL(StartSearchPic(QString)),this,SLOT(PicClicked(QString)));
    connect(this,SIGNAL(StartSearchArt(QString)),this,SLOT(ArticleClicked(QString)));
    connect(this,SIGNAL(StartSearchRank(QString,QString)),this,SLOT(rank(QString,QString)));
    connect(this,SIGNAL(StartSearchTag(QString)),this,SLOT(TagSearch(QString)));
    //加载最近排行榜
    QDateTime begin_time = QDateTime::currentDateTime();
    QString date = begin_time.addDays(-2).toString("yyyy-MM-dd");
    QDate current_date =QDate::currentDate();
    ui->dateEdit->setDate(current_date.addDays(-2));
    if (m_set.rankAuto)emit StartSearchRank("day",date);
}

Pixiv::~Pixiv()
{
    delete ui;
    delete request;
    delete m_accessManager;
}
//接收函数
void Pixiv::finishedSlot(QNetworkReply *reply)
{

     if (reply->error() == QNetworkReply::NoError)
     {
         QByteArray bytes = reply->readAll();
         emit Requestover(&bytes);
     }
     else
     {
         qDebug()<<"handle errors here";
         QVariant statusCodeV = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
         //statusCodeV是HTTP服务器的相应码，reply->error()是Qt定义的错误码，可以参考QT的文档

         msg->setText("found error ....code: " + QString::number(statusCodeV.toInt())+" " + QString::number((int)reply->error()));
         print("found error ....code: " + QString::number(statusCodeV.toInt())+" " + QString::number((int)reply->error()));
         msg->adjustSize();
         msg->show();
     }
     reply->deleteLater();
}

void Pixiv::on_toolButton_clicked()
{
    int choice = ui->comboBox->currentIndex();
    switch (choice) {
    case 0:emit StartSearchPic(ui->lineEdit->text());break;
    case 1:emit StartSearchArt(ui->lineEdit->text());break;
    case 2:
        m_page_tag = 1;
        m_TagY = 50;
        for (int i = 0;i < (int)m_tagPic.size();i++){
            m_tagPic[i]->setParent(NULL);
            delete m_tagPic[i];
        }
        m_tagPic.clear();
        emit StartSearchTag(ui->lineEdit->text());
        break;
    }

}

void Pixiv::DealSearchContent(QByteArray* content){

    for (int i = 0;i < (int)m_Piclabel.size();i++){
        m_Piclabel[i]->setParent(NULL);
        delete m_Piclabel[i];
    }
    m_Piclabel.clear();

    print("开始处理图片数据;");

    m_imgs.clear();
    QJsonDocument doc = QJsonDocument::fromJson(*content);
    QJsonObject obj = doc.object().value("illust").toObject();

    m_imgs.SetPicID(obj.value("id").toInt());
    m_imgs.SetStatus_code(200);
    m_imgs.SetPage_count(obj.value("page_count").toInt());
    m_imgs.SetUserID(obj.value("user").toObject().value("id").toInt());
    m_imgs.SetPicName(obj.value("title").toString());
    m_imgs.SetUserName(obj.value("user").toObject().value("name").toString());

    ui->pname->setText(m_imgs.GetPicName());
    ui->pname->setCursorPosition(0);
    ui->pid->setText(QString::number(m_imgs.GetPicID()));
    ui->aname->setText(m_imgs.GetUserName());
    ui->aname->setCursorPosition(0);
    ui->aid->setText(QString::number(m_imgs.GetUserID()));
    ui->page->setText(QString::number(m_imgs.GetPage_count()));

    QString quality = "";
    switch (m_set.PicQuality) {
    case 0:
        quality = "square_medium";
        break;
    case 1:
        quality = "medium";
        break;
    case 2:
        quality = "large";
        break;
    default:
        break;
    }

    if (m_imgs.GetPage_count() == 1){
        if (m_set.PicQuality == 3)
            m_imgs.PushAUrlToMedium(ChanHost(obj.value("meta_single_page").toObject().value("original_image_url").toString(),FIRST_HOST));
        else m_imgs.PushAUrlToMedium(ChanHost(obj.value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
        m_imgs.PushAUrlToOrigin(ChanHost(obj.value("meta_single_page").toObject().value("original_image_url").toString(),FIRST_HOST));
    }
    else {
        QJsonArray array = obj.value("meta_pages").toArray();
        for (int i = 0;i < array.size();i++){
            m_imgs.PushAUrlToMedium(ChanHost(array.at(i).toObject().value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
            m_imgs.PushAUrlToOrigin(ChanHost(array.at(i).toObject().value("image_urls").toObject().value("original").toString(),FIRST_HOST));
        }
    }

    print("开始请求图片数据!");

    QNetworkAccessManager manager;
    QEventLoop loop;
    int Consty = 200;//下一张插画图片的y坐标
    ui->scrollAreaWidgetContents->setMinimumHeight(250);

    for (int i = 0;i < (int)m_imgs.GetPage_count();i++){
        request->setUrl(QUrl(m_imgs.square_medium[i]));
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply *reply = manager.get(*request);
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
       //开启子事件循环
        loop.exec();
        QByteArray Pic = reply->readAll();

        QImage img;
        img.loadFromData(Pic);
        //imgsContent.push_back(&img);
        QLabel *label = new QLabel(ui->scrollAreaWidgetContents);
        m_Piclabel.push_back(label);
        int width = img.width(),height = img.height();
        double times = 1;
        while(width > 450 || height > 400){
            times += 0.1;
            width = img.width() / times;
            height = img.height() / times;
        }
        label->setFixedHeight(height);
        label->setFixedWidth(width);
        label->setScaledContents(true);
        ui->scrollAreaWidgetContents->setMinimumHeight(Consty + height + 50);
        label->move(491 / 2 - width / 2,Consty);
        Consty += height + 10;
        label->setPixmap(Radius(QPixmap::fromImage(img),width,height));
        label->setStyleSheet("border: 1px solid #808080;border-radius: 15px;");
        label->show();
        print("显示图片成功!");
        reply->deleteLater();
    }
}

void Pixiv::print(QString info){
    ui->textEdit->append(GetTime() + ": " + info);
    qDebug() << GetTime() + ": " << info;
}

QString ChanHost(QString url,QString hosts)
{
    int s = url.indexOf("://",0,Qt::CaseInsensitive) + 3;
    int e = url.indexOf("/",s,Qt::CaseInsensitive);
    QString host = url.mid(s,e - s);
    return url.replace(host,hosts);
}
//查找随机图
void Pixiv::on_toolButton_2_clicked()
{
    //准备
    print("查找随机插画。");

    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealRadomPic(QByteArray*)));
    //开始
    QString r18 = "";
    if (m_set.R18)r18 = "&r18=true";

    QString url = "https://api.loli.st/pixiv/random.php?type=json" + r18;
    request->setUrl(QUrl(url));
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//随机图片处理
void Pixiv::DealRadomPic(QByteArray* content){
    print("正常处理随机插画!");
    QJsonObject doc = QJsonDocument::fromJson(*content).object();
    QString id = doc.value("illust_id").toString();
    emit StartSearchPic(id);
}
//下载
void Pixiv::on_toolButton_3_clicked()
{
    print("开始下载图片数据!");
    ui->progressBar_2->setMinimum(0);
    ui->progressBar_2->setMaximum(m_imgs.GetPage_count() * 1000);
    ui->progressBar_2->setValue(0);
    std::vector<qint64>lasts(m_imgs.GetPage_count(),0);
    for (int i = 0;i < int(m_imgs.GetPage_count());i++){
        QNetworkAccessManager *manager = new QNetworkAccessManager;

        connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply* reply)mutable{
            QByteArray bytes = reply->readAll();
            int last = m_imgs.original[i].lastIndexOf("/");
            QString name = m_imgs.original[i].mid(last);
            QFile file;
            file.setFileName(m_set.path + name);
            file.open(QIODevice::WriteOnly);
            file.write(bytes);
            file.close();
            ui->progressBar_2->setValue(m_imgs.GetPage_count() * 1000);
            print("下载完成！ " + name);
            reply->deleteLater();
        });
        //发出请求
        request->setUrl(QUrl(m_imgs.original[i]));
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply* reply = manager->get(*request);
        connect(reply, &QNetworkReply::downloadProgress, [=](qint64 now,qint64 all)mutable{
            double a = now,b = all;
            ui->progressBar_2->setValue(ui->progressBar_2->value() + (double)((a-lasts[i]) / b) * 1000);
            lasts[i] = now;
        });
   }
}
//读取设置
void ReadSetting(){
    QFile file;
    file.setFileName("./pix_config.json");
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray content = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(content);
        QJsonObject obj = doc.object();
        m_set.path = obj.value("path").toString();
        m_set.R18 = obj.value("R18").toBool();
        m_set.PicQuality = obj.value("PicQuality").toInt();
        m_set.rankAuto = obj.value("rankAuto").toBool();

    }
    else {
        file.close();
        file.open(QIODevice::WriteOnly);
        QJsonObject obj;
        obj.insert("path",m_set.path);
        obj.insert("R18",m_set.R18);
        obj.insert("rankAuto",m_set.rankAuto);
        obj.insert("PicQuality",m_set.PicQuality);
        QJsonDocument document=QJsonDocument(obj);
        QByteArray array = document.toJson();
        file.write(array);
    }
    file.close();
}
//保存设置
void Pixiv::on_toolButton_4_clicked()
{
    QFile file("./pix_config.json");
    file.open(QIODevice::WriteOnly);
    QJsonObject obj;
    obj.insert("path",ui->lineEdit_2->text());
    obj.insert("R18",ui->checkBox->isChecked());
    obj.insert("rankAuto",ui->checkBox_2->isChecked());
    obj.insert("PicQuality",ui->comboBox_2->currentIndex());
    m_set.path = ui->lineEdit_2->text();
    m_set.R18 = ui->checkBox->isChecked();
    m_set.PicQuality = ui->comboBox_2->currentIndex();
    m_set.rankAuto = ui->checkBox_2->isChecked();
    QJsonDocument document=QJsonDocument(obj);
    QByteArray array = document.toJson();
    file.write(array);
    file.close();

    print("保存设置成功！");

    msg->setText("保存成功！");
    msg->show();

}
//浏览文件夹
void Pixiv::on_liulan_clicked()
{
    QString paths = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, "选择保存路径", QDir::currentPath()));
    ui->lineEdit_2->setText(paths);
}
//画师
void Pixiv::ArticleClicked(QString id){
    print("开始查找画师:" + ui->lineEdit->text() + "。");
    //准备
    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealAritcleContent(QByteArray*)));
    //开始
    QString url = "https://pixiviz.pwp.app/api/v1/user/detail?id=" + id;
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//处理画师信息
void Pixiv::DealAritcleContent(QByteArray* content){
    for (int i = 0;i < (int)m_Articlelabel.size();i++){
        m_Articlelabel[i]->setParent(NULL);
        delete m_Articlelabel[i];
    }
    m_Articlelabel.clear();
    for (int i = 0;i < (int)m_Pics.size();i++){
        m_Pics[i]->setParent(NULL);
        delete m_Pics[i];
    }
    m_Pics.clear();
    m_page_pic = 1;
    ui->scrollAreaWidgetContents_3->setMinimumHeight(ui->scrollArea_3->height());

    print("正在处理画师数据。");
    QJsonDocument doc = QJsonDocument::fromJson(*content);
    QJsonObject obj = doc.object();
    QString id = QString::number(obj.value("user").toObject().value("id").toInt());
    QString name = obj.value("user").toObject().value("name").toString();
    QString headUrl = ChanHost(obj.value("user").toObject().value("profile_image_urls").toObject().value("medium").toString(),FIRST_HOST);
    QString comment = obj.value("user").toObject().value("comment").toString();
    int PicNum = obj.value("profile").toObject().value("total_illusts").toInt();
    int MangaNum = obj.value("profile").toObject().value("total_manga").toInt();
    int novelsNum = obj.value("profile").toObject().value("total_novels").toInt();

    for (int i = 0;i < 8;i++){
        QLabel *label = new QLabel(ui->scrollAreaWidgetContents_3);
        m_Articlelabel.push_back(label);
    }
    //画师名称
    m_Articlelabel[0]->setText(name);
    m_Articlelabel[0]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[0]->adjustSize();
    m_Articlelabel[0]->move(491 / 2 - m_Articlelabel[0]->width() / 2,220);
    m_Articlelabel[0]->show();

    int y1 = m_Articlelabel[0]->pos().y() + m_Articlelabel[0]->height() + 20;
    m_Articlelabel[2]->setText("漫画");
    m_Articlelabel[2]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[2]->adjustSize();
    m_Articlelabel[2]->move(m_Articlelabel[0]->pos().x() + m_Articlelabel[0]->width() / 2 - m_Articlelabel[2]->width() / 2,y1);
    m_Articlelabel[2]->show();

    m_Articlelabel[1]->setText("插画");
    m_Articlelabel[1]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[1]->adjustSize();
    m_Articlelabel[1]->move(m_Articlelabel[2]->pos().x() - 40 - m_Articlelabel[2]->width() / 2,y1);
    m_Articlelabel[1]->show();

    m_Articlelabel[3]->setText("小说");
    m_Articlelabel[3]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[3]->adjustSize();
    m_Articlelabel[3]->move(m_Articlelabel[2]->pos().x() + m_Articlelabel[2]->width() / 2 + 40,y1);
    m_Articlelabel[3]->show();


    int y2 = m_Articlelabel[1]->pos().y() + m_Articlelabel[1]->height();
    m_Articlelabel[4]->setText(QString::number(PicNum));
    m_Articlelabel[4]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[4]->adjustSize();
    m_Articlelabel[4]->move(m_Articlelabel[1]->pos().x() + m_Articlelabel[1]->width() / 2 - m_Articlelabel[4]->width() / 2,y2);
    m_Articlelabel[4]->show();

    m_Articlelabel[5]->setText(QString::number(MangaNum));
    m_Articlelabel[5]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[5]->adjustSize();
    m_Articlelabel[5]->move(m_Articlelabel[2]->pos().x() + m_Articlelabel[2]->width() / 2 - m_Articlelabel[5]->width() / 2,y2);
    m_Articlelabel[5]->show();

    m_Articlelabel[6]->setText(QString::number(novelsNum));
    m_Articlelabel[6]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    m_Articlelabel[6]->adjustSize();
    m_Articlelabel[6]->move(m_Articlelabel[3]->pos().x() + m_Articlelabel[3]->width() / 2  - m_Articlelabel[6]->width() / 2,y2);
    m_Articlelabel[6]->show();

    m_Articlelabel[7]->setText(comment);
    m_Articlelabel[7]->setStyleSheet("font-family: 黑体;font-size: 14px;font-weight: 400;color: #da7a85;line-height: 24px;");
    m_Articlelabel[7]->adjustSize();
    m_Articlelabel[7]->move(491 / 2 - m_Articlelabel[7]->width() / 2,m_Articlelabel[6]->pos().y() + m_Articlelabel[6]->height() + 20);
    m_Articlelabel[7]->show();
    m_ArtY = m_Articlelabel[7]->pos().y() + m_Articlelabel[7]->height() + 20;

    ui->scrollAreaWidgetContents_3->setMinimumHeight(m_ArtY);

    QNetworkAccessManager manager;
    QEventLoop loop;
    //发出请求
    request->setUrl(QUrl(headUrl));
    print("开始载入画师头像。");
    request->setRawHeader("referer","https://www.pixiv.net");
    QNetworkReply *reply = manager.get(*request);
   //请求结束并下载完成后，退出子事件循环
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
   //开启子事件循环
    loop.exec();
    QByteArray headPic = reply->readAll();
    print("画师头像大小: "  + QString::number(headPic.size()));

    QImage toux;
    toux.loadFromData(headPic);
    int width = toux.width(),height = toux.height();
    double times = 1;
    while(width > 200 || height > 200){
        times += 0.1;
        width = toux.width() / times;
        height = toux.height() / times;
    }
    delete m_AHead;
    m_AHead = NULL;

    m_AHead = new QLabel(ui->scrollAreaWidgetContents_3);
    m_AHead->setFixedHeight(height);
    m_AHead->setFixedWidth(width);

    QPixmap pixmap(width,height);//准备装图片的矩形框
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0,0,width, height,width/2,width/2);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, width, height, QPixmap::fromImage(toux));//将显示图片的pixmap画入矩形框内
    qDebug() << width << " "<<height;

    m_AHead->setPixmap(pixmap);
    m_AHead->setScaledContents(true);
    m_AHead->move(491 / 2 - width / 2,20);
    m_AHead->show();
    print("载入画师头像完成！");

    reply->deleteLater();

    print("开始查找画师插画。");
    m_page_pic = 1;
    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealArticlePic(QByteArray*)));
    QString url = "https://pixiviz.pwp.app/api/v1/user/illusts?id=" + ui->lineEdit->text() + "&page=" + QString::number(m_page_pic);

    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);

}
//处理画师插画
void Pixiv::DealArticlePic(QByteArray* content){
    print("开始处理画师插画数据。");
    QJsonDocument doc = QJsonDocument::fromJson(*content);
    QJsonObject obj = doc.object();

    QJsonArray AtPics = obj.value("illusts").toArray();
    if (AtPics.size() == 0){
        msg->setText("已经是最后一页！！");
        msg->show();
        return;
    }
    for (int i = 0;i < AtPics.size();i++){
        QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value("medium").toString(),FIRST_HOST);
        Mylabel* label = new Mylabel(ui->scrollAreaWidgetContents_3);

        connect(label,&Mylabel::clicked,[=](){
            ui->tabWidget->setCurrentIndex(1);
            disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
            connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealSearchContent(QByteArray*)));
            QJsonObject newobj;
            newobj.insert("illust",AtPics.at(i).toObject());
            QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);
            emit Requestover(&byte);
        });

        m_Pics.push_back(label);

        QNetworkAccessManager manager;
        QEventLoop loop;
        //发出请求
        request->setUrl(QUrl(ChanHost(url,FIRST_HOST)));
        print("开始载入画师插画。");
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply *reply = manager.get(*request);
       //请求结束并下载完成后，退出子事件循环
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
       //开启子事件循环
        loop.exec();
        QByteArray Pic = reply->readAll();

        QImage toux;
        toux.loadFromData(Pic);
        int width = toux.width(),height = toux.height();
        double times = 1;
        while(width > 450 || height > 400){
            times += 0.1;
            width = toux.width() / times;
            height = toux.height() / times;
        }
        label->setFixedHeight(height);
        label->setFixedWidth(width);
        label->Seteffect();
        label->setCursor(Qt::PointingHandCursor);
        label->SetName(AtPics.at(i).toObject().value("title").toString());
        label->SetPage(QString::number(AtPics.at(i).toObject().value("page_count").toInt()));
        label->setStyleSheet("border: 1px solid #808080;border-radius: 15px;");
        label->setPixmap(Radius(QPixmap::fromImage(toux),width,height));
        label->setScaledContents(true);
        ui->scrollAreaWidgetContents_3->setMinimumHeight(m_ArtY + label->height() + 50);
        label->move(491 / 2 - width / 2,m_ArtY);
        label->show();

        m_ArtY += label->height() + 10;
        print("正在载入插画。");

        reply->deleteLater();
    }

    connect(ui->pushButton,&QPushButton::clicked,[=](){
        print("下一页。");
        m_page_pic++;
        ui->pushButton->setVisible(false);
        disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
        connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealArticlePic(QByteArray*)));
        QString url = "https://pixiviz.pwp.app/api/v1/user/illusts?id=" + ui->lineEdit->text() + "&page=" + QString::number(m_page_pic);

        request->setUrl(QUrl(url));
        request->setRawHeader("Referer","https://pixiviz.pwp.app/");
        request->setRawHeader("user-agent",
                              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
        m_accessManager->get(*request);
    });

    ui->pushButton->setVisible(true);
    ui->pushButton->move(20,m_Pics[m_Pics.size() - 1]->pos().y() + m_Pics[m_Pics.size() - 1]->height());
}
//插画爬取
void Pixiv::PicClicked(QString id){
    //准备

    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//情况Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealSearchContent(QByteArray*)));

    //开始
    print("开始获取插画。");
    QString url = "https://pixiviz.pwp.app/api/v1/illust/detail?id=" + id;
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//获取当前时间
QString GetTime(){
    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("hh:mm:ss");
    return current_date;
}

void Pixiv::on_textEdit_textChanged()
{
    ui->textEdit->moveCursor(QTextCursor::End);
}
//排行榜时间被选择
void Pixiv::on_pushButton_2_clicked()
{
    QString date = ui->dateEdit->text();
    for(int i = 0;i < date.size();i++){
        if (date.at(i) == "-" && i == date.size() - 2)
        {
            date.remove(i,1);
            date.insert(i,"-0");
            break;
        }
        if (date.at(i) == "-" && date.at(i + 2) == "-"){
            date.remove(i,1);
            date.insert(i,"-0");
        }
    }
    int choice = ui->comboBox_3->currentIndex();
    QString mode = "";
    switch (choice) {
    case 0:
        mode = "day";
        break;
    case 1:
        mode = "week";
        break;
    case 2:
        mode = "month";
        break;
    default:
        break;
    }
    m_RankY = 50;
    ui->pushButton_3->setVisible(false);
    ui->scrollAreaWidgetContents_4->setMinimumHeight(ui->scrollArea_4->height());
    emit StartSearchRank(mode,date);
}
//排行榜获取
void Pixiv::rank(QString mode,QString time){

    print("开始爬取排行榜插画。");
    QString url = "https://pixiviz.pwp.app/api/v1/illust/rank?mode=" + mode + "&date=" + time + "&page=1";
    m_next_url_rank = url;
    int page = int(url.at(m_next_url_rank.size() - 1).toLatin1()) - 47;
    m_next_url_rank.chop(url.size() - 1 - url.lastIndexOf("="));
    m_next_url_rank = m_next_url_rank + QString::number(page);
    for (int i = 0;i < (int)m_rankPic.size();i++){
        m_rankPic[i]->setParent(NULL);
        delete m_rankPic[i];
    }
    m_rankPic.clear();

    print("开始请求");
    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//情况Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealRank(QByteArray*)));
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//处理排行榜数据
void Pixiv::DealRank(QByteArray* content){
    print("开始处理排行榜数据。");
    QJsonDocument doc = QJsonDocument::fromJson(*content);
    QJsonObject obj = doc.object();

    QJsonArray AtPics = obj.value("illusts").toArray();
    if (AtPics.size() == 0){
        msg->setText("已经是最后一页！！");
        msg->show();
        return;
    }
    for (int i = 0;i < AtPics.size();i++){
        QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value("medium").toString(),FIRST_HOST);
        Mylabel* label = new Mylabel(ui->scrollAreaWidgetContents_4);
        connect(label,&Mylabel::clicked,[=](){
            ui->tabWidget->setCurrentIndex(1);
            disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
            connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealSearchContent(QByteArray*)));
            QJsonObject newobj;
            newobj.insert("illust",AtPics.at(i).toObject());
            QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);
            emit Requestover(&byte);
        });
        m_rankPic.push_back(label);

        QNetworkAccessManager manager;
        QEventLoop loop;
        //发出请求
        request->setUrl(QUrl(ChanHost(url,FIRST_HOST)));
        print("开始载入排行榜插画。");
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply *reply = manager.get(*request);
       //请求结束并下载完成后，退出子事件循环
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
       //开启子事件循环
        loop.exec();
        QByteArray Pic = reply->readAll();

        QImage toux;
        toux.loadFromData(Pic);
        int width = toux.width(),height = toux.height();
        double times = 1;
        while(width > 450 || height > 400){
            times += 0.1;
            width = toux.width() / times;
            height = toux.height() / times;
        }
        label->setFixedWidth(width);
        label->setFixedHeight(height);
        label->Seteffect();
        label->setCursor(Qt::PointingHandCursor);
        label->SetName(AtPics.at(i).toObject().value("title").toString());
        label->SetPage(QString::number(AtPics.at(i).toObject().value("page_count").toInt()));
        label->setStyleSheet("border: 1px solid #808080;border-radius: 15px;");
        label->setPixmap(Radius(QPixmap::fromImage(toux),width,height));
        label->setScaledContents(true);
        ui->scrollAreaWidgetContents_4->setMinimumHeight(m_RankY + label->height() + 50);
        label->move(491 / 2 - width / 2,m_RankY);
        label->show();

        m_RankY += label->height() + 10;
        reply->deleteLater();
    }
    connect(ui->pushButton_3,&QPushButton::clicked,[=](){
        QString url = m_next_url_rank;
        int page = int(url.at(url.size() - 1).toLatin1()) - 47;
        m_next_url_rank.chop(url.size() - 1 - url.lastIndexOf("="));
        m_next_url_rank = url + QString::number(page);
        ui->pushButton_3->setVisible(false);

        disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//情况Requestover的所有槽
        connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealRank(QByteArray*)));
        request->setUrl(QUrl(url));
        request->setRawHeader("Referer","https://pixiviz.pwp.app/rank");
        request->setRawHeader("user-agent",
                              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
        m_accessManager->get(*request);
    });
    ui->pushButton_3->setVisible(true);
    ui->pushButton_3->move(20,m_rankPic[m_rankPic.size() - 1]->pos().y() + m_rankPic[m_rankPic.size() - 1]->height());
}

QPixmap Pixiv::Radius(QPixmap pic,int width,int height){
    QPixmap pixmap(width,height);//准备装图片的矩形框
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0,0,width, height,15,15);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, width, height, pic);//将显示图片的pixmap画入矩形框内
    return pixmap;
}

void Pixiv::TagSearch(QString word)
{
    print("开始搜索Tag");
    QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
    QByteArray encoded = utf8->fromUnicode(QString::fromUtf8(word.toUtf8())).toPercentEncoding();
    QString base_url = "https://pixiviz.pwp.app/api/v1/illust/search?word=" + QString(encoded);
    QString url = base_url + "&page=" + QString::number(m_page_tag);
    QNetworkAccessManager *manager = new QNetworkAccessManager;
    //发出请求
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/search/" + encoded);

    manager->get(*request);

    connect(manager,&QNetworkAccessManager::finished,[=](QNetworkReply* reply){
        print("开始处理Tag信息.");
        QByteArray content = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(content);
        QJsonObject obj = doc.object();

        QJsonArray AtPics = obj.value("illusts").toArray();
        if (AtPics.size() == 0){
            msg->setText("已经是最后一页！！");
            msg->show();
            return;
        }
        for (int i = 0;i < AtPics.size();i++){
            QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value("medium").toString(),FIRST_HOST);
            Mylabel* label = new Mylabel(ui->scrollAreaWidgetContents_5);
            connect(label,&Mylabel::clicked,[=](){
                ui->tabWidget->setCurrentIndex(1);
                disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
                connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealSearchContent(QByteArray*)));
                QJsonObject newobj;
                newobj.insert("illust",AtPics.at(i).toObject());
                QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);
                emit Requestover(&byte);
            });

            m_tagPic.push_back(label);

            QNetworkAccessManager manager;
            QEventLoop loop;
            request->setUrl(QUrl(ChanHost(url,FIRST_HOST)));
            request->setRawHeader("referer","https://www.pixiv.net");
            QNetworkReply *replys = manager.get(*request);
            QObject::connect(replys, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();
            QByteArray Pic = replys->readAll();

            QImage toux;
            toux.loadFromData(Pic);
            int width = toux.width(),height = toux.height();
            double times = 1;
            while(width > 450 || height > 400){
                times += 0.1;
                width = toux.width() / times;
                height = toux.height() / times;
            }
            label->setFixedWidth(width);
            label->setFixedHeight(height);
            label->Seteffect();
            label->setCursor(Qt::PointingHandCursor);
            label->setScaledContents(true);
            label->setStyleSheet("border: 1px solid #808080;border-radius: 15px;");
            label->SetName(AtPics.at(i).toObject().value("title").toString());
            label->SetPage(QString::number(AtPics.at(i).toObject().value("page_count").toInt()));
            label->setPixmap(Radius(QPixmap::fromImage(toux),width,height));
            label->move(491 / 2 - width / 2,m_TagY);
            label->show();
            ui->scrollAreaWidgetContents_5->setMinimumHeight(m_TagY + label->height() + 50);
            m_TagY += label->height() + 10;
            replys->deleteLater();

        }
        reply->deleteLater();
        ui->pushButton_4->setVisible(true);
        ui->pushButton_4->move(20,m_tagPic[m_tagPic.size() - 1]->pos().y() + m_tagPic[m_tagPic.size() - 1]->height());
        connect(ui->pushButton_4,&QPushButton::clicked,[=](){
            ui->pushButton_4->setVisible(false);
            m_page_tag++;
            emit StartSearchTag(word);
        });
    });
}
