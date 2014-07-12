#include "widget.h"
#include "ui_widget.h"
#include <QtGui>
#include <QtSql>
#include <QtEndian>
#include <QFileDialog>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_dataFileButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Open Data File"),
                                                    "E:/",
                                                    tr("Data File(data.dat)"));
    if(filename.isEmpty())
        return;
    ui->dataFile->setText(filename);
}

void Widget::on_targetButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(
                this,
                tr("Export to..."),
                "E:/Data",
                tr("SQLite (*.sqlite)"));

    if(filename.isEmpty())
        return;

    ui->targetFolder->setText(filename);
}

QString decode(QByteArray &data)
{
    if(data.size()<=17)
        return QString();
    quint8* cursor = (quint8*)data.data();
    cursor++;
    quint32 blocks = ntohl(*(quint32*)cursor);
    cursor+=4;

    quint32 datetime1 = *(quint32*)cursor;
    cursor+=4;
    quint32 datetime2 = *(quint32*)cursor;
    cursor+=4;

    auto count = ntohl(*(int*)cursor);
    cursor+=4;

    if(!count)
        return QString();

    //cursor+=0x19;
    QString result;
    for(int i=0;i<count;i++)
    {
        quint32 ttime;
        quint8 *pttime = (quint8*)&ttime;
        pttime[0]=cursor[3];
        pttime[1]=cursor[2];
        pttime[2]=cursor[1];
        pttime[3]=cursor[0];
        auto time = QDateTime::fromTime_t(ttime);
        auto timePart = time.toLocalTime().toString("yyyy-MM-dd hh:mm:ss");
        result += timePart+",";
        cursor+=4;
        double doubleValue;
        quint8 *pvalue=(quint8*)&doubleValue;
        pvalue[0]=cursor[3];
        pvalue[1]=cursor[2];
        pvalue[2]=cursor[1];
        pvalue[3]=cursor[0];
        pvalue[4]=cursor[7];
        pvalue[5]=cursor[6];
        pvalue[6]=cursor[5];
        pvalue[7]=cursor[4];

        result += QString::number(doubleValue,'f',6)+"\n";
        cursor += 8;
    }
    return result;
}

QString decode2(QByteArray &data)
{
    if(data.size()<=17)
        return QString();
    quint8* cursor = (quint8*)data.data();
    cursor++;
    quint32 blocks = ntohl(*(quint32*)cursor);
    //Q_ASSERT(blocks==1);
    cursor+=4;

    quint32 datetime1 = *(quint32*)cursor;
    cursor+=4;
    quint32 datetime2 = *(quint32*)cursor;
    cursor+=4;

    auto count = ntohl(*(int*)cursor);
    cursor+=4;

    if(!count)
        return QString();

    QString result;
    for(int i=0;i<count;i++)
    {
        quint32 ttime = ntohl(*(quint32*)cursor);
        auto time = QDateTime::fromTime_t(ttime);
        auto timePart = time.toLocalTime().toString("yyyy-MM-dd hh:mm:ss");
        result += timePart;
        cursor+=4;

        quint32 countv = ntohl(*(quint32*)cursor);
        cursor+=4;

        for(int i=0;i<countv;i++)
        {
            result += ",";
            double doubleValue;
            *((quint32*)&doubleValue)=ntohl(*(quint32*)cursor);
            cursor+=4;
            *((quint32*)&doubleValue+1)=ntohl(*(quint32*)cursor);
            cursor+=4;
            result += QString::number(doubleValue,'f',6);
        }
        result += "\n";
    }
    return result;
}

void Widget::on_startButton_clicked()
{
    if(ui->dataFile->text().isEmpty())
        return;
    if(ui->targetFolder->text().isEmpty())
        return;

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(ui->dataFile->text());
    if(!database.open())
    {
        database.close();
        return;
    }

    QSqlQuery query;
    query.exec("SELECT * FROM t_stockly_double");

    while(query.next())
    {
        auto name = query.value(0).toString();
        auto value = query.value(1).toByteArray();
        auto nameList = name.split("-");
        Q_ASSERT(nameList.size()==5);

        auto ttime = nameList.at(4).toInt();
        auto time = QDateTime::fromTime_t(ttime);
        auto timePart = time.toLocalTime().toString("yyyyMMdd");
        QString filename =
                ui->targetFolder->text()+"/"+
                timePart+"-"+
                nameList.at(1)+"-"+
                nameList.at(2)+"-"+
                nameList.at(3)+".csv";
        QFile file(filename);
        if(file.exists())
            continue;

        auto decoded = decode(value);

        if(decoded.isEmpty())
            continue;

        bool success;
        success=file.open(QIODevice::WriteOnly|QIODevice::Text);
        if(!success)
        {
            QString reason = file.errorString();
            ui->msg->append(reason);
            ui->msg->append(filename);
            return;
        }
        file.write(decoded.toLocal8Bit());
        file.close();
        ui->msg->append(filename);
        qApp->processEvents();
    }

    query.exec("SELECT * FROM t_stockly_vdouble");

    while(query.next())
    {
        auto name = query.value(0).toString();
        auto value = query.value(1).toByteArray();
        auto nameList = name.split("-");
        Q_ASSERT(nameList.size()==5);

        auto ttime = nameList.at(4).toInt();
        auto time = QDateTime::fromTime_t(ttime);
        auto timePart = time.toLocalTime().toString("yyyyMMdd");
        QString filename =
                ui->targetFolder->text()+"/"+
                timePart+"-"+
                nameList.at(1)+"-"+
                nameList.at(2)+"-"+
                nameList.at(3)+".csv";
        QFile file(filename);
        if(file.exists())
            continue;

        auto decoded = decode2(value);

        if(decoded.isEmpty())
            continue;

        bool success;
        success=file.open(QIODevice::WriteOnly|QIODevice::Text);
        if(!success)
        {
            QString reason = file.errorString();
            ui->msg->append(reason);
            ui->msg->append(filename);
            return;
        }
        file.write(decoded.toLocal8Bit());
        file.close();
        ui->msg->append(filename);
        qApp->processEvents();
    }
    qDebug()<<"Done!";
    ui->msg->append("Done!");
}
