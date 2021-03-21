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

/*全局变量*/

const QString VERSION = "1.1";
ImgInfo imgs;//图片信息
std::vector<QLabel*>Piclabel;//上一次显示的图片label指针
std::vector<QLabel*>Articlelabel;//上一次显示的作家label指针
QLabel* AHead;//画师头像
Setting set;//设置信息
int ArtY = 0;//画师插画的下一个y坐标
int RankY = 50;//排行榜插画下一个y坐标
int page_pic = 1;//画师插画页数
QString next_url_rank = "";//排行榜下一页url
std::vector<Mylabel*>Pics;//画师插画
std::vector<Mylabel*>rankPic;//排行榜插画

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
    ui->pushButton_3->setVisible(false);
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
    ui->lineEdit_2->setText(set.path);
    ui->checkBox->setChecked(set.R18);
    ui->comboBox_2->setCurrentIndex(set.PicQuality);
    //信息框
    msg->setWindowTitle("提示:");
    msg->setIcon(QMessageBox::Question);
    ui->textEdit_2->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->pushButton->setVisible(false);
    //连接信号
    connect(this,SIGNAL(StartSearchPic(QString)),this,SLOT(PicClicked(QString)));
    connect(this,SIGNAL(StartSearchArt(QString)),this,SLOT(ArticleClicked(QString)));
    connect(this,SIGNAL(StartSearchRank(QString,QString)),this,SLOT(rank(QString,QString)));
    //加载最近排行榜
    QDateTime begin_time = QDateTime::currentDateTime();
    QString date = begin_time.addDays(-2).toString("yyyy-MM-dd");
    QDate current_date =QDate::currentDate();
    ui->dateEdit->setDate(current_date.addDays(-2));
    //emit StartSearchRank("day",date);
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
    if (choice == 0){
        emit StartSearchPic(ui->lineEdit->text());
    }
    else
    {
        emit StartSearchArt(ui->lineEdit->text());
    }
}

void Pixiv::DealSearchContent(QByteArray* content){
    for (int i = 0;i < (int)Piclabel.size();i++){
        Piclabel[i]->setParent(NULL);
        delete Piclabel[i];
    }
    Piclabel.clear();

    print("开始处理图片数据;");

    imgs.clear();
    QJsonDocument doc = QJsonDocument::fromJson(*content);
    QJsonObject obj = doc.object().value("illust").toObject();
    QString headUrl = ChanHost(obj.value("user").toObject().value("profile_image_urls").toObject().value("medium").toString(),FIRST_HOST);
    imgs.SetPicID(obj.value("id").toInt());
    imgs.SetStatus_code(200);
    imgs.SetPage_count(obj.value("page_count").toInt());
    imgs.SetUserID(obj.value("user").toObject().value("id").toInt());
    imgs.SetPicName(obj.value("title").toString());
    imgs.SetUserName(obj.value("user").toObject().value("name").toString());

    //载入画师头像
    Mylabel *artpic = new Mylabel(ui->scrollAreaWidgetContents);
    QNetworkAccessManager manager;
    QEventLoop loop;
    //发出请求
    request->setUrl(QUrl(headUrl));
    print("开始载入画师头像。");
    request->setRawHeader("referer","https://www.pixiv.net");
    QNetworkReply *reply = manager.get(*request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray headPic = reply->readAll();
    QImage toux;
    toux.loadFromData(headPic);
    int width = toux.width(),height = toux.height();
    double times = 1;
    while(width > 50 || height > 100){
        times += 0.1;
        width = toux.width() / times;
        height = toux.height() / times;
    }
    artpic->setPixmap(Radius(QPixmap::fromImage(toux),width,height));
    artpic->setScaledContents(true);
    artpic->setStyleSheet("border-radius: " + QString::number(width/2) + "px;");
    artpic->move(ui->label->pos().x(),(ui->label_4->pos().y() + ui->label->height())/2 - artpic->height() / 2);
    artpic->show();
    print("载入画师头像完成！");
    reply->deleteLater();
    //画师头像被点击
    QString UserId = QString::number(imgs.GetUserID());
    connect(artpic,&Mylabel::clicked,[=](){
        emit StartSearchArt(UserId);
        ui->tabWidget->setCurrentIndex(2);
    });
    //载入画师名称
    Mylabel *artName = new Mylabel(ui->scrollAreaWidgetContents);
    artName->setText(imgs.GetUserName());
    artName->setStyleSheet("font-family: 黑体;font-size: 23px;text-align: center;color: #da7a85;font-weight: 600;");
    artName->adjustSize();
    artName->setFixedHeight(artpic->height());
    artName->move(artpic->pos().x() + artpic->width() + 20,artpic->pos().y());
    artName->show();

    QString quality = "";
    switch (set.PicQuality) {
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

    if (imgs.GetPage_count() == 1){
        if (set.PicQuality == 3)
            imgs.PushAUrlToMedium(ChanHost(obj.value("meta_single_page").toObject().value("original_image_url").toString(),FIRST_HOST));
        else imgs.PushAUrlToMedium(ChanHost(obj.value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
        imgs.PushAUrlToOrigin(ChanHost(obj.value("meta_single_page").toObject().value("original_image_url").toString(),FIRST_HOST));
    }
    else {
        QJsonArray array = obj.value("meta_pages").toArray();
        for (int i = 0;i < array.size();i++){
            imgs.PushAUrlToMedium(ChanHost(array.at(i).toObject().value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
            imgs.PushAUrlToOrigin(ChanHost(array.at(i).toObject().value("image_urls").toObject().value("original").toString(),FIRST_HOST));
        }
    }

    std::vector<QImage*>imgsContent(imgs.GetPage_count(),NULL);//读取的图片数据
    for (int i = (int)imgs.GetPage_count() - 1;i >= 0;i--){
        request->setUrl(QUrl(imgs.square_medium[i]));
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply *reply = manager.get(*request);
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
       //开启子事件循环
        loop.exec();
        QByteArray Pic = reply->readAll();

        QImage img;
        img.loadFromData(Pic);
        imgsContent[i] = &img;
        reply->deleteLater();
    }

    int i = 0;
    Mylabel *pics = new Mylabel(ui->scrollAreaWidgetContents);

    width = imgsContent[i]->width();
    height = imgsContent[i]->height();
    times = 1;
    while(width > ui->label->pos().x() || height >450){
        times += 0.1;
        width = imgsContent[i]->width() / times;
        height = imgsContent[i]->height() / times;
    }
    pics->setFixedHeight(height);
    pics->setFixedWidth(width);
    pics->setScaledContents(true);
    pics->move(0,0);
    pics->setPixmap(Radius(QPixmap::fromImage(*imgsContent[i]),width,height));
    pics->setStyleSheet("border: 1px solid #808080;border-radius: 15px;");
    pics->show();
    pics->setCursor(Qt::PointingHandCursor);
    print("显示图片成功!");
    //上一页
    Mylabel *last = new Mylabel(ui->scrollAreaWidgetContents);
    last->setText("上一页");
    last->setStyleSheet("font-family: 黑体;font-size: 15px;text-align: center;color: #da7a85;font-weight: 600;");
    last->adjustSize();
    last->move(pics->pos().x(),pics->pos().y() + pics->height());
    //下一页
    Mylabel *next = new Mylabel(ui->scrollAreaWidgetContents);
    next->setText("下一页");
    next->setStyleSheet("font-family: 黑体;font-size: 15px;text-align: center;color: #da7a85;font-weight: 600;");
    next->adjustSize();
    next->move(pics->pos().x() + pics->width() - next->width(),last->pos().y());
    next->show();
    //页数
    Mylabel *pages = new Mylabel(ui->scrollAreaWidgetContents);
    pages->setText("1 / " + QString::number(imgsContent.size()));
    pages->setStyleSheet("font-family: 黑体;font-size: 15px;text-align: center;color: #da7a85;font-weight: 600;");
    pages->adjustSize();
    pages->move(pics->width() / 2 - pages->width() / 2,last->y());
    pages->show();
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
    if (set.R18)r18 = "&r18=true";

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
    ui->progressBar_2->setMaximum(imgs.GetPage_count() * 1000);
    ui->progressBar_2->setValue(0);
    std::vector<qint64>lasts(imgs.GetPage_count(),0);
    for (int i = 0;i < int(imgs.GetPage_count());i++){
        QNetworkAccessManager *manager = new QNetworkAccessManager;

        connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply* reply)mutable{
            QByteArray bytes = reply->readAll();
            int last = imgs.original[i].lastIndexOf("/");
            QString name = imgs.original[i].mid(last);
            QFile file;
            file.setFileName(set.path + name);
            file.open(QIODevice::WriteOnly);
            file.write(bytes);
            file.close();
            ui->progressBar_2->setValue(imgs.GetPage_count() * 1000);
            print("下载完成！ " + name);
            reply->deleteLater();
        });
        //发出请求
        request->setUrl(QUrl(imgs.original[i]));
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
        set.path = obj.value("path").toString();
        set.R18 = obj.value("R18").toBool();
        set.PicQuality = obj.value("PicQuality").toInt();
    }
    else {
        file.close();
        file.open(QIODevice::WriteOnly);
        QJsonObject obj;
        obj.insert("path",set.path);
        obj.insert("R18",set.R18);
        obj.insert("PicQuality",set.PicQuality);
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
    obj.insert("PicQuality",ui->comboBox_2->currentIndex());
    set.path = ui->lineEdit_2->text();
    set.R18 = ui->checkBox->isChecked();
    set.PicQuality = ui->comboBox_2->currentIndex();
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
    for (int i = 0;i < (int)Articlelabel.size();i++){
        Articlelabel[i]->setParent(NULL);
        delete Articlelabel[i];
    }
    Articlelabel.clear();
    for (int i = 0;i < (int)Pics.size();i++){
        Pics[i]->setParent(NULL);
        delete Pics[i];
    }
    Pics.clear();
    page_pic = 1;
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
        Articlelabel.push_back(label);
    }
    //画师名称
    Articlelabel[0]->setText(name);
    Articlelabel[0]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[0]->adjustSize();
    Articlelabel[0]->move(491 / 2 - Articlelabel[0]->width() / 2,220);
    Articlelabel[0]->show();

    int y1 = Articlelabel[0]->pos().y() + Articlelabel[0]->height() + 20;
    Articlelabel[2]->setText("漫画");
    Articlelabel[2]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[2]->adjustSize();
    Articlelabel[2]->move(Articlelabel[0]->pos().x() + Articlelabel[0]->width() / 2 - Articlelabel[2]->width() / 2,y1);
    Articlelabel[2]->show();

    Articlelabel[1]->setText("插画");
    Articlelabel[1]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[1]->adjustSize();
    Articlelabel[1]->move(Articlelabel[2]->pos().x() - 40 - Articlelabel[2]->width() / 2,y1);
    Articlelabel[1]->show();

    Articlelabel[3]->setText("小说");
    Articlelabel[3]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[3]->adjustSize();
    Articlelabel[3]->move(Articlelabel[2]->pos().x() + Articlelabel[2]->width() / 2 + 40,y1);
    Articlelabel[3]->show();


    int y2 = Articlelabel[1]->pos().y() + Articlelabel[1]->height();
    Articlelabel[4]->setText(QString::number(PicNum));
    Articlelabel[4]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[4]->adjustSize();
    Articlelabel[4]->move(Articlelabel[1]->pos().x() + Articlelabel[1]->width() / 2 - Articlelabel[4]->width() / 2,y2);
    Articlelabel[4]->show();

    Articlelabel[5]->setText(QString::number(MangaNum));
    Articlelabel[5]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[5]->adjustSize();
    Articlelabel[5]->move(Articlelabel[2]->pos().x() + Articlelabel[2]->width() / 2 - Articlelabel[5]->width() / 2,y2);
    Articlelabel[5]->show();

    Articlelabel[6]->setText(QString::number(novelsNum));
    Articlelabel[6]->setStyleSheet("font-family: 黑体;font-size: 26px;text-align: center;color: #da7a85;font-weight: 600;");
    Articlelabel[6]->adjustSize();
    Articlelabel[6]->move(Articlelabel[3]->pos().x() + Articlelabel[3]->width() / 2  - Articlelabel[6]->width() / 2,y2);
    Articlelabel[6]->show();

    Articlelabel[7]->setText(comment);
    Articlelabel[7]->setStyleSheet("font-family: 黑体;font-size: 14px;font-weight: 400;color: #da7a85;line-height: 24px;");
    Articlelabel[7]->adjustSize();
    Articlelabel[7]->move(491 / 2 - Articlelabel[7]->width() / 2,Articlelabel[6]->pos().y() + Articlelabel[6]->height() + 20);
    Articlelabel[7]->show();
    ArtY = Articlelabel[7]->pos().y() + Articlelabel[7]->height() + 20;

    ui->scrollAreaWidgetContents_3->setMinimumHeight(ArtY);

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
    delete AHead;
    AHead = NULL;

    AHead = new QLabel(ui->scrollAreaWidgetContents_3);
    AHead->setFixedHeight(height);
    AHead->setFixedWidth(width);
    AHead->setPixmap(QPixmap::fromImage(toux));
    AHead->setScaledContents(true);
    AHead->setStyleSheet("border: 1px solid #808080;border-radius: " + QString::number(width/2) + "px;");
    AHead->move(491 / 2 - width / 2,20);
    AHead->show();
    print("载入画师头像完成！");

    reply->deleteLater();

    print("开始查找画师插画。");
    page_pic = 1;
    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealArticlePic(QByteArray*)));
    QString url = "https://pixiviz.pwp.app/api/v1/user/illusts?id=" + ui->lineEdit->text() + "&page=" + QString::number(page_pic);

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
    QString next = obj.value("next_url").toString();

    if (next == ""){
        msg->setText("已经是最后一页！！");
        msg->show();
        return;
    }
    QJsonArray AtPics = obj.value("illusts").toArray();

    for (int i = 0;i < AtPics.size();i++){
        //int id = AtPics.at(i).toObject().value("id").toInt();
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
        Pics.push_back(label);

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
        ui->scrollAreaWidgetContents_3->setMinimumHeight(ArtY + label->height() + 50);
        label->move(491 / 2 - width / 2,ArtY);
        label->show();

        ArtY += label->height() + 10;
        print("正在载入插画。");

        reply->deleteLater();
    }
    ui->pushButton->setVisible(true);
    ui->pushButton->move(20,Pics[Pics.size() - 1]->pos().y() + Pics[Pics.size() - 1]->height());
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
//下一页
void Pixiv::on_pushButton_clicked()
{
    print("下一页。");
    page_pic++;
    ui->pushButton->setVisible(false);
    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealArticlePic(QByteArray*)));
    QString url = "https://pixiviz.pwp.app/api/v1/user/illusts?id=" + ui->lineEdit->text() + "&page=" + QString::number(page_pic);

    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
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
    RankY = 50;
    ui->pushButton_3->setVisible(false);
    ui->scrollAreaWidgetContents_4->setMinimumHeight(ui->scrollArea_4->height());
    emit StartSearchRank(mode,date);
}
//排行榜获取
void Pixiv::rank(QString mode,QString time){

    print("开始爬取排行榜插画。");
    QString url = "https://pixiviz.pwp.app/api/v1/illust/rank?mode=" + mode + "&date=" + time + "&page=1";
    next_url_rank = url;
    int page = int(url.at(next_url_rank.size() - 1).toLatin1()) - 47;
    next_url_rank.chop(url.size() - 1 - url.lastIndexOf("="));
    next_url_rank = next_url_rank + QString::number(page);
    for (int i = 0;i < (int)rankPic.size();i++){
        rankPic[i]->setParent(NULL);
        delete rankPic[i];
    }
    rankPic.clear();

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
    QString next = obj.value("next_url").toString();

    if (next == ""){
        msg->setText("已经是最后一页！！");
        msg->show();
        return;
    }
    QJsonArray AtPics = obj.value("illusts").toArray();
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
        rankPic.push_back(label);

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
        ui->scrollAreaWidgetContents_4->setMinimumHeight(RankY + label->height() + 50);
        label->move(491 / 2 - width / 2,RankY);
        label->show();

        RankY += label->height() + 10;
        reply->deleteLater();
    }
    ui->pushButton_3->setVisible(true);
    ui->pushButton_3->move(20,rankPic[rankPic.size() - 1]->pos().y() + rankPic[rankPic.size() - 1]->height());
}
//排行榜下一页
void Pixiv::on_pushButton_3_clicked()
{
    QString url = next_url_rank;
    int page = int(url.at(url.size() - 1).toLatin1()) - 47;
    next_url_rank.chop(url.size() - 1 - url.lastIndexOf("="));
    next_url_rank = url + QString::number(page);
    ui->pushButton_3->setVisible(false);

    disconnect(this, SIGNAL(Requestover(QByteArray*)),0,0);//情况Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray*)),this,SLOT(DealRank(QByteArray*)));
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/rank");
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
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
