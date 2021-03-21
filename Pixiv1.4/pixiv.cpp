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
#include <QScrollBar>
#include <algorithm>
#define FIRST_HOST "210.140.92.138"
#define Message(A,B) QMessageBox::information(NULL, A,B ,QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)


/*全局变量*/
SOFTWARE_VERSION VERSION = "1.5";
ImgInfo *m_imgs = new ImgInfo;//图片信息
std::vector<Mylabel*>m_Articlelabel;//上一次显示的作家label指针
Mylabel* m_AHead;//画师头像
Setting *m_set = new Setting;//设置信息
PicContent *Art_PicContent = new PicContent;
PicContent *Rank_PicContent = new PicContent;
PicContent *Tag_PicContent = new PicContent;
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
        if (QMessageBox::Yes == Message("提示","网络未连接或者已有新版本，请前往https://ealodi.xyz查看。"))
        {
                QDesktopServices::openUrl(QString("https://ealodi.xyz"));
        }
    }
    //读取设置内容   
    ReadSetting();
    ui->lineEdit_2->setText(m_set->path);
    ui->checkBox->setChecked(m_set->R18);
    ui->checkBox_2->setChecked(m_set->rankAuto);
    ui->comboBox_2->setCurrentIndex(m_set->PicQuality);
    //连接信号
    connect(this,SIGNAL(StartSearchPic(QString)),this,SLOT(PicClicked(QString)));
    connect(this,SIGNAL(StartSearchArt(QString)),this,SLOT(ArticleClicked(QString)));
    connect(this,SIGNAL(StartSearchRank(QString,QString)),this,SLOT(rank(QString,QString)));
    connect(this,SIGNAL(StartSearchTag(QString)),this,SLOT(TagSearch(QString)));
    //加载最近排行榜
    if (m_set->rankAuto){
        QDateTime begin_time = QDateTime::currentDateTime();
        QString date = begin_time.addDays(-2).toString("yyyy-MM-dd");
        QDate current_date =QDate::currentDate();
        ui->dateEdit->setDate(current_date.addDays(-2));
        emit StartSearchRank("day",date);
    }
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
         emit Requestover(bytes);
     }
     else
     {
         qDebug()<<"handle errors here";
         QVariant statusCodeV = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
         //statusCodeV是HTTP服务器的相应码，reply->error()是Qt定义的错误码，可以参考QT的文档
        qDebug() << reply->error();
         Message("Request Error: ",QString::number(statusCodeV.toInt())+" " + QString::number((int)reply->error()));
         print("Request Error: " + QString::number(statusCodeV.toInt())+" " + QString::number((int)reply->error()));
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
        emit StartSearchTag(ui->lineEdit->text());
        break;
    }

}

void Pixiv::DealSearchContent(QByteArray& content){
    cleanLast(ui->scrollAreaWidgetContents);
    m_imgs->clear();
    QJsonDocument doc = QJsonDocument::fromJson(content);
    QJsonObject obj = doc.object().value("illust").toObject();

    m_imgs->SetPicID(obj.value("id").toInt());
    m_imgs->SetStatus_code(200);
    m_imgs->SetPage_count(obj.value("page_count").toInt());
    m_imgs->SetUserID(obj.value("user").toObject().value("id").toInt());
    m_imgs->SetPicName(obj.value("title").toString());
    m_imgs->SetUserName(obj.value("user").toObject().value("name").toString());

    ui->pname->setText(m_imgs->GetPicName());
    ui->pname->setCursorPosition(0);
    ui->pid->setText(QString::number(m_imgs->GetPicID()));
    ui->aname->setText(m_imgs->GetUserName());
    ui->aname->setCursorPosition(0);
    ui->aid->setText(QString::number(m_imgs->GetUserID()));
    ui->page->setText(QString::number(m_imgs->GetPage_count()));

    QString quality = m_set->GetPicQuality();

    if (m_imgs->GetPage_count() == 1){
        m_imgs->PushAUrlToMedium(ChanHost(obj.value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
        m_imgs->PushAUrlToOrigin(ChanHost(obj.value("meta_single_page").toObject().value("original_image_url").toString(),FIRST_HOST));
    }
    else {
        QJsonArray array = obj.value("meta_pages").toArray();
        for (int i = 0;i < array.size();i++){
            m_imgs->PushAUrlToMedium(ChanHost(array.at(i).toObject().value("image_urls").toObject().value(quality).toString(),FIRST_HOST));
            m_imgs->PushAUrlToOrigin(ChanHost(array.at(i).toObject().value("image_urls").toObject().value("original").toString(),FIRST_HOST));
        }
    }

    QNetworkAccessManager manager;
    QEventLoop loop;

    for (int i = 0;i < (int)m_imgs->GetPage_count();i++){
        request->setUrl(QUrl(m_imgs->square_medium[i]));
        request->setRawHeader("referer","https://www.pixiv.net");
        QNetworkReply *reply = manager.get(*request);
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
       //开启子事件循环
        loop.exec();
        QByteArray Pic = reply->readAll();

        QImage img;
        img.loadFromData(Pic);
        int y = GetLargestY(ui->scrollAreaWidgetContents);
        Mylabel *label = new Mylabel(ui->scrollAreaWidgetContents);

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
        label->move(491 / 2 - width / 2,y + 10);
        ui->scrollAreaWidgetContents->setMinimumHeight(GetLargestY(ui->scrollAreaWidgetContents) + 50);
        label->setPixmap(Radius(img,15));
        label->setStyleSheet("border: 2px solid #808080;border-radius: 15px;");
        label->show();
        print("显示图片成功!");
        reply->deleteLater();
    }
}

void Pixiv::print(QString info){
    ui->textEdit->append(GetTime() + ": " + info);
    //qDebug() << GetTime() + ": " << info;
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
    disconnect(this, SIGNAL(Requestover(QByteArray&)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray&)),this,SLOT(DealRadomPic(QByteArray&)));
    //开始
    QString r18 = "";
    if (m_set->R18)r18 = "&r18=true";

    QString url = "https://api.loli.st/pixiv/random.php?type=json" + r18;
    request->setUrl(QUrl(url));
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//随机图片处理
void Pixiv::DealRadomPic(QByteArray& content){
    print("正常处理随机插画!");
    QJsonObject doc = QJsonDocument::fromJson(content).object();
    QString id = doc.value("illust_id").toString();
    emit StartSearchPic(id);
}
//下载
void Pixiv::on_toolButton_3_clicked()
{
    print("开始下载图片数据!");
    ui->progressBar_2->setMinimum(0);
    ui->progressBar_2->setMaximum(m_imgs->GetPage_count() * 1000);
    ui->progressBar_2->setValue(0);
    std::vector<qint64>lasts(m_imgs->GetPage_count(),0);
    for (int i = 0;i < int(m_imgs->GetPage_count());i++){
        QNetworkAccessManager *manager = new QNetworkAccessManager;

        connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply* reply)mutable{
            QByteArray bytes = reply->readAll();
            int last = m_imgs->original[i].lastIndexOf("/");
            QString name = m_imgs->original[i].mid(last);
            QFile file;
            file.setFileName(m_set->path + name);
            file.open(QIODevice::WriteOnly);
            file.write(bytes);
            file.close();
            ui->progressBar_2->setValue(m_imgs->GetPage_count() * 1000);
            print("下载完成！ " + name);
            reply->deleteLater();
        });
        //发出请求
        request->setUrl(QUrl(m_imgs->original[i]));
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
        m_set->path = obj.value("path").toString();
        m_set->R18 = obj.value("R18").toBool();
        m_set->PicQuality = obj.value("PicQuality").toInt();
        m_set->rankAuto = obj.value("rankAuto").toBool();

    }
    else {
        file.close();
        file.open(QIODevice::WriteOnly);
        QJsonObject obj;
        obj.insert("path",m_set->path);
        obj.insert("R18",m_set->R18);
        obj.insert("rankAuto",m_set->rankAuto);
        obj.insert("PicQuality",m_set->PicQuality);
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
    m_set->path = ui->lineEdit_2->text();
    m_set->R18 = ui->checkBox->isChecked();
    m_set->PicQuality = ui->comboBox_2->currentIndex();
    m_set->rankAuto = ui->checkBox_2->isChecked();
    QJsonDocument document=QJsonDocument(obj);
    QByteArray array = document.toJson();
    file.write(array);
    file.close();

    print("保存设置成功！");

    Message("提示","保存成功!");

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
    ui->scrollAreaWidgetContents_3->setMaximumHeight(ui->scrollArea_3->height());
    //准备
    disconnect(this, SIGNAL(Requestover(QByteArray&)),0,0);//清空Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray&)),this,SLOT(DealAritcleContent(QByteArray&)));
    //开始
    QString url = "https://pixiviz.pwp.app/api/v1/user/detail?id=" + id;;
    request->setUrl(QUrl(url));
    request->setRawHeader("Referer","https://pixiviz.pwp.app/artist/" + id.toUtf8());
    request->setRawHeader("user-agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
    m_accessManager->get(*request);
}
//处理画师信息
void Pixiv::DealAritcleContent(QByteArray& content){
    cleanLast(ui->scrollAreaWidgetContents_3);

    Art_PicContent->clear();

    connect(ui->scrollArea_3->verticalScrollBar(),&QScrollBar::valueChanged,this,&Pixiv::Scroll_3_Bar_Down);
    QJsonDocument doc = QJsonDocument::fromJson(content);
    QJsonObject obj = doc.object();
    QString id = QString::number(obj.value("user").toObject().value("id").toInt());
    QString name = obj.value("user").toObject().value("name").toString();
    QString headUrl = ChanHost(obj.value("user").toObject().value("profile_image_urls").toObject().value("medium").toString(),FIRST_HOST);
    QString comment = obj.value("user").toObject().value("comment").toString();
    int PicNum = obj.value("profile").toObject().value("total_illusts").toInt();
    int MangaNum = obj.value("profile").toObject().value("total_manga").toInt();
    int novelsNum = obj.value("profile").toObject().value("total_novels").toInt();

    m_Articlelabel.clear();
    for (int i = 0;i < 8;i++){
        Mylabel *label = new Mylabel(ui->scrollAreaWidgetContents_3);
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
    ui->scrollAreaWidgetContents_3->setMinimumHeight(m_Articlelabel[7]->pos().y() + m_Articlelabel[7]->height() + 20);
    if (ui->scrollAreaWidgetContents_3->height() <= ui->scrollArea_3->height()){
        ui->scrollAreaWidgetContents_3->setMinimumHeight(ui->scrollArea_3->height() + 10);
    }
    QNetworkAccessManager *manager = new QNetworkAccessManager;
    //发出请求
    request->setUrl(QUrl(headUrl));
    print("载入画师头像。");
    request->setRawHeader("referer","https://www.pixiv.net");
    manager->get(*request);
    connect(manager,&QNetworkAccessManager::finished,[=](QNetworkReply *reply){

        QByteArray contents = reply->readAll();
        print("画师头像大小: "  + QString::number(contents.size()));

        QImage toux;
        toux.loadFromData(contents);
        int width = toux.width(),height = toux.height();
        double times = 1;
        while(width > 200 || height > 200){
            times += 0.1;
            width = toux.width() / times;
            height = toux.height() / times;
        }

        m_AHead = new Mylabel(ui->scrollAreaWidgetContents_3);
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


        m_AHead->setPixmap(pixmap);
        m_AHead->setScaledContents(true);
        m_AHead->move(491 / 2 - width / 2,20);
        m_AHead->show();
        print("载入画师头像完成！");

        reply->deleteLater();
    });

    int page = 1;
    while(page < PicNum / 30 + 2){
        QNetworkAccessManager *manager_3 = new QNetworkAccessManager;
        QString url = "https://pixiviz.pwp.app/api/v1/user/illusts?id=" + id + "&page=" + QString::number(page);
        request->setUrl(QUrl(url));
        request->setRawHeader("Referer","https://pixiviz.pwp.app/artist/" + id.toUtf8());
        request->setRawHeader("user-agent",
                              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
        connect(manager_3,&QNetworkAccessManager::finished,[=](QNetworkReply *reply)mutable{
            QByteArray contents = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(contents);
            QJsonObject obj = doc.object();

            QJsonArray AtPics = obj.value("illusts").toArray();
            qDebug() << AtPics.size();
            if (AtPics.size() == 0){
                //disconnect(ui->scrollArea_3->verticalScrollBar(),&QScrollBar::valueChanged,this,&Pixiv::Scroll_3_Bar_Down);
                return;
            }
            QString quality = m_set->GetPicQuality();

            for (int i = 0;i < AtPics.size();i++){
                QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value(quality).toString(),FIRST_HOST);
                request->setUrl(QUrl(url));
                QNetworkAccessManager *manager_2 = new QNetworkAccessManager;
                request->setRawHeader("Referer","https://www.pixiv.net");
                manager_2->get(*request);

                connect(manager_2, &QNetworkAccessManager::finished, [=](QNetworkReply *replys){
                    QByteArray content = replys->readAll();
                    QImage image;
                    image.loadFromData(content);
                    QPixmap b = Radius(image,15);
                    DisPlay_PicContent img;
                    img.image = b.toImage();
                    img.title = AtPics.at(i).toObject().value("title").toString();
                    img.page_count = QString::number(AtPics.at(i).toObject().value("page_count").toInt());
                    QJsonObject newobj;
                    newobj.insert("illust",AtPics.at(i).toObject());
                    QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);

                    img.content = byte;
                    Art_PicContent->push_back(img);
                    replys->deleteLater();
                });
            }
            reply->deleteLater();
        });
        manager_3->get(*request);
        page++;
    }

}
//插画爬取
void Pixiv::PicClicked(QString id){
    //准备
    ui->scrollAreaWidgetContents->setMaximumHeight(ui->scrollArea->height());
    disconnect(this, SIGNAL(Requestover(QByteArray&)),0,0);//情况Requestover的所有槽
    connect(this,SIGNAL(Requestover(QByteArray&)),this,SLOT(DealSearchContent(QByteArray&)));

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
    emit StartSearchRank(mode,date);
}
//排行榜获取
void Pixiv::rank(QString mode,QString time){
    cleanLast(ui->scrollAreaWidgetContents_4);
    ui->scrollAreaWidgetContents_4->setMaximumHeight(ui->scrollArea_4->height() + 10);
    connect(ui->scrollArea_4->verticalScrollBar(),&QScrollBar::valueChanged,this,&Pixiv::Scroll_4_Bar_Down);
    int page = 1;
    while(page <= 3){
        QString url = "https://pixiviz.pwp.app/api/v1/illust/rank?mode=" + mode + "&date=" + time + "&page=" + QString::number(page);
        QNetworkAccessManager *manager = new QNetworkAccessManager;
        connect(manager,&QNetworkAccessManager::finished,[=](QNetworkReply *reply)mutable{
            QByteArray content = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(content);
            QJsonObject obj = doc.object();

            QJsonArray AtPics = obj.value("illusts").toArray();
            if (AtPics.size() == 0){
                //disconnect(ui->scrollArea_3->verticalScrollBar(),&QScrollBar::valueChanged,this,&Pixiv::Scroll_3_Bar_Down);
                return;
            }
            QString quality = m_set->GetPicQuality();
            for (int i = 0;i < AtPics.size();i++){
                QNetworkAccessManager *manager_2 = new QNetworkAccessManager;
                QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value(quality).toString(),FIRST_HOST);
                request->setUrl(QUrl(ChanHost(url,FIRST_HOST)));
                request->setRawHeader("referer","https://www.pixiv.net");
                connect(manager_2, &QNetworkAccessManager::finished, [=](QNetworkReply *replys){
                    QByteArray content = replys->readAll();
                    QImage image;
                    image.loadFromData(content);
                    DisPlay_PicContent img;
                    img.image = Radius(image,15).toImage();
                    img.title = AtPics.at(i).toObject().value("title").toString();
                    img.page_count = QString::number(AtPics.at(i).toObject().value("page_count").toInt());
                    QJsonObject newobj;
                    newobj.insert("illust",AtPics.at(i).toObject());
                    QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);
                    img.content = byte;
                    Rank_PicContent->push_back(img);
                    replys->deleteLater();
                });
                manager_2->get(*request);
            }
            reply->deleteLater();
        });
        request->setUrl(QUrl(url));
        request->setRawHeader("Referer","https://pixiviz.pwp.app");
        request->setRawHeader("user-agent",
                              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/78.0.3904.87 Safari/537.36");
        manager->get(*request);
        page++;
    }
}

QPixmap Pixiv::Radius(QImage &pic,int radius){
    int width = pic.width();
    int height = pic.height();

    QPixmap pixmap(width,height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0,0,width, height,radius,radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, width, height, QPixmap::fromImage(pic));//将显示图片的pixmap画入矩形框内
    return pixmap;
}

void Pixiv::TagSearch(QString word)
{
    cleanLast(ui->scrollAreaWidgetContents_5);
    ui->scrollAreaWidgetContents_5->setMaximumHeight(ui->scrollArea_5->height() + 10);
    connect(ui->scrollArea_5->verticalScrollBar(),&QScrollBar::valueChanged,this,&Pixiv::Scroll_5_Bar_Down);
    int page = 1;
    while (page <= 3)
    {
        QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
        QByteArray encoded = utf8->fromUnicode(QString::fromUtf8(word.toUtf8())).toPercentEncoding();
        QString base_url = "https://pixiviz.pwp.app/api/v1/illust/search?word=" + QString(encoded);
        QString url = base_url + "&page=" + QString::number(page);
        QNetworkAccessManager *manager = new QNetworkAccessManager;
        connect(manager,&QNetworkAccessManager::finished,[=](QNetworkReply* reply)mutable{
            QByteArray content = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(content);
            QJsonObject obj = doc.object();

            QJsonArray AtPics = obj.value("illusts").toArray();
            if (AtPics.size() == 0){
                return;
            }
            for (int i = 0;i < AtPics.size();i++){
                QNetworkAccessManager *manager_2 = new QNetworkAccessManager;
                QString url = ChanHost(AtPics.at(i).toObject().value("image_urls").toObject().value("medium").toString(),FIRST_HOST);
                request->setUrl(QUrl(ChanHost(url,FIRST_HOST)));
                request->setRawHeader("referer","https://www.pixiv.net");
                manager_2->get(*request);
                connect(manager_2, &QNetworkAccessManager::finished, [=](QNetworkReply *replys){
                    QByteArray content = replys->readAll();
                    QImage image;
                    image.loadFromData(content);
                    DisPlay_PicContent img;
                    img.image = Radius(image,15).toImage();
                    img.title = AtPics.at(i).toObject().value("title").toString();
                    img.page_count = QString::number(AtPics.at(i).toObject().value("page_count").toInt());
                    QJsonObject newobj;
                    newobj.insert("illust",AtPics.at(i).toObject());
                    QByteArray byte = QJsonDocument(newobj).toJson(QJsonDocument::Compact);
                    img.content = byte;
                    Tag_PicContent->push_back(img);
                    replys->deleteLater();
                });
            }
            reply->deleteLater();
        });
        //发出请求
        request->setUrl(QUrl(url));
        request->setRawHeader("Referer","https://pixiviz.pwp.app/search/" + encoded);
        manager->get(*request);
        page++;
    }
}

QString Setting::GetPicQuality(){

    QString quality = "";
    switch (PicQuality) {
    case 0:
        quality = "square_medium";
        break;
    case 1:
        quality = "medium";
        break;
    case 2:
        quality = "large";
        break;
    }
    return quality;
}
//画师下一张滚动插画
void Pixiv::Scroll_3_Bar_Down(int yUp){
    PutPicInScroll(Art_PicContent,ui->scrollAreaWidgetContents_3,491,450,400,yUp);
}
//排行榜下一张滚动插画
void Pixiv::Scroll_4_Bar_Down(int yUp){
    PutPicInScroll(Rank_PicContent,ui->scrollAreaWidgetContents_4,491,450,400,yUp);
}
//tag下一张滚动插画
void Pixiv::Scroll_5_Bar_Down(int yUp){
    PutPicInScroll(Tag_PicContent,ui->scrollAreaWidgetContents_5,491,450,400,yUp);
}

int Pixiv::GetLargestY(QWidget *scrollContent){
    QObjectList children = scrollContent->children();
    int Large = 0;
    for (int i = 0;i < children.size();i++){
        int y = qobject_cast<QWidget*>(children.at(i))->pos().y() + qobject_cast<QWidget*>(children.at(i))->height();
        Large =  y > Large ? y : Large;
    }
    return Large;
}

void Pixiv::PutPicInScroll(PicContent*picContent,QWidget*scrollContent,int x,int _width,int _height,int yUp){
    if (picContent->size() == 0)return;
    QWidget *area = qobject_cast<QWidget*>(scrollContent->parent());
    if (yUp + area->height() == scrollContent->height()){
        int y = GetLargestY(scrollContent) + 10;
        if (picContent->begin() != picContent->end()){
            QImage image = picContent->begin()->image;
            Mylabel* label = new Mylabel(scrollContent);
            QByteArray content = picContent->begin()->content;
            connect(label,&Mylabel::clicked,[=](){
                ui->tabWidget->setCurrentIndex(1);
                QByteArray contents = content;
                qDebug() << contents.size();
                disconnect(this, SIGNAL(Requestover(QByteArray&)),0,0);//清空Requestover的所有槽
                connect(this,SIGNAL(Requestover(QByteArray&)),this,SLOT(DealSearchContent(QByteArray&)));

                emit Requestover(contents);

            });
            int width = image.width(),height = image.height();
            double times = 1;
            while(width > _width || height > _height){
                times += 0.1;
                width = image.width() / times;
                height = image.height() / times;
            }
            label->setFixedWidth(width);
            label->setFixedHeight(height);
            label->SetName(picContent->begin()->title);
            label->SetPage(picContent->begin()->page_count);
            label->Seteffect();
            label->setCursor(Qt::PointingHandCursor);
            label->setStyleSheet("border: 2px solid #808080;border-radius: 15px;");
            label->setPixmap(QPixmap::fromImage(image));
            label->setScaledContents(true);
            if (y >= area->height() - 10)
            scrollContent->setMinimumHeight(y + label->height() + 30);
            label->move(x / 2 - width / 2,y);
            label->show();

            picContent->erase(picContent->begin());
        }
    }
}
