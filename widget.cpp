#include "widget.h"
#include "ui_widget.h"
#include <QtGui>
#include <QMessageBox>
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

int decode(QByteArray &data,
           QSqlQuery &query,
           QString &table,
           int id)
{

    int rowCount = 0;

    if(data.size() <= 17)
        return rowCount;
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
        return rowCount;

    query.exec("CREATE TABLE IF NOT EXISTS [t_" + table + "]"
               "(id INTEGER NOT NULL REFERENCES datas(id) ON DELETE CASCADE, "
               "timestamp INTEGER NOT NULL, value1 REAL NOT NULL)");

    query.prepare("INSERT INTO [t_" + table + "] VALUES "
                  "(:id, :timestamp, :data)");
    for(int i=0;i<count;i++)
    {
        quint32 ttime = ntohl(*(int*)cursor);
        cursor+=4;
        double doubleValue;
        quint32 *pvalue=(quint32*)&doubleValue;
        *pvalue = ntohl(*(int*)cursor);
        cursor+=4;
        pvalue++;
        *pvalue = ntohl(*(int*)cursor);
        cursor+=4;
        query.bindValue(":id", id);
        query.bindValue(":timestamp", ttime);
        query.bindValue(":data", doubleValue);
        bool result = query.exec();
        if(!result)
            qDebug() << query.lastError().text();
        rowCount = i;
    }
    return rowCount;
}

int decode2(QByteArray &data,
            QSqlQuery &query,
            QString &table,
            int id)
{
    int rowCount = 0;
    if(data.size()<=17)
        return rowCount;
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
        return rowCount;

    auto cloneCursor = cursor;
    cloneCursor += 4;
    auto colCount = ntohl(*(quint32*)cloneCursor);

    QString queryString = "CREATE TABLE IF NOT EXISTS [t_" + table + "]"
            "(id INTEGER NOT NULL REFERENCES datas(id) ON DELETE CASCADE, "
            "timestamp INTEGER NOT NULL";
    for(int i=0; i<colCount; i++)
        queryString += QString(", value%1 REAL NOT NULL").arg(i+1);
    queryString += ");";
    query.exec(queryString);

    queryString = "INSERT INTO [t_" + table + "] VALUES "
            "(:id, :timestamp";
    for(int i=0; i<colCount; i++)
        queryString += QString(", :data%1").arg(i+1);
    queryString += ");";

    query.prepare(queryString);
    for(int i=0;i<count;i++)
    {
        quint32 ttime = ntohl(*(quint32*)cursor);
        cursor+=4;

        quint32 countv = ntohl(*(quint32*)cursor);
        if(countv != colCount)
        {
            query.exec("DROP TABLE [t_" + table + "]");
            rowCount = -1;
            break;
        }
        cursor+=4;

        query.bindValue(":id", id);
        query.bindValue(":timestamp", ttime);
        for(int i=0;i<countv;i++)
        {
            double doubleValue;
            *((quint32*)&doubleValue)=ntohl(*(quint32*)cursor);
            cursor+=4;
            *((quint32*)&doubleValue+1)=ntohl(*(quint32*)cursor);
            cursor+=4;
            query.bindValue(QString(":data%1").arg(i+1), doubleValue);
        }
        bool result = query.exec();
        if(!result)
            qDebug() << query.lastError().text();
        rowCount = i;
    }
    return rowCount;
}

void Widget::on_startButton_clicked()
{
    if(ui->dataFile->text().isEmpty())
        return;
    if(ui->targetFolder->text().isEmpty())
        return;

    if(!OpenDatabase(database, "src", ui->dataFile->text()))
        return;
    if(!OpenDatabase(destDatabase, "dest", ui->targetFolder->text()))
    {
        database.close();
        return;
    }
    destDatabase.transaction();

    QSqlQuery destQuery(destDatabase);
    InitInfoTable(destQuery);

    QSqlQuery query(database);
    query.exec("SELECT * FROM t_stockly_double");
    Decode(query, destQuery, 1);

    query.exec("SELECT * FROM t_stockly_vdouble");
    Decode(query, destQuery, 2);

    destDatabase.commit();
    destDatabase.close();
    database.close();

    qDebug()<<"Done!";
    ui->msg->append("Done!");
}

bool Widget::OpenDatabase(QSqlDatabase &database, QString name, QString dbName)
{
    bool result = true;
    database = QSqlDatabase::addDatabase("QSQLITE", name);
    database.setDatabaseName(dbName);
    if(!database.open())
    {
        result = false;
        QMessageBox msgBox;
        msgBox.setText("Open Database Error:" + database.lastError().text());
        msgBox.exec();
    }
    return result;
}

void Widget::InitInfoTable(QSqlQuery &query)
{
    query.exec("PRAGMA foreign_keys = ON;");
    query.exec("CREATE TABLE IF NOT EXISTS datas("
               "id INTEGER PRIMARY KEY, "
               "market TEXT NOT NULL, "
               "product TEXT NOT NULL, "
               "data TEXT NOT NULL, "
               "dataid TEXT NOT NULL, "
               "timestamp INTEGER NOT NULL,"
               "datacount INTEGER DEFAULT 0);");
    query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idatas ON datas("
               "market, product, data, dataid, timestamp);");
    query.exec("CREATE INDEX IF NOT EXISTS idatas_t ON datas(timestamp);");
    query.exec("CREATE INDEX IF NOT EXISTS idatas_count ON datas(datacount);");
    query.exec("DELETE FROM datas WHERE timestamp = 0;");
}

void Widget::Decode(QSqlQuery &query, QSqlQuery &destQuery, int selector)
{
    while(query.next())
    {
        auto name = query.value(0).toString();
        auto nameList = name.split("-");
        auto value = query.value(1).toByteArray();
        Q_ASSERT(nameList.size()==5);

        QString market      = nameList.at(0).toLocal8Bit();
        QString product     = nameList.at(1).toLocal8Bit();
        QString table       = nameList.at(2).toLocal8Bit();
        QString subTable    = nameList.at(3).toLocal8Bit();
        auto ttime          = nameList.at(4).toInt();

        destQuery.prepare("INSERT INTO datas "
                      "VALUES (NULL, :market, :product, :data, :dataid, :timestamp, 0)");
        destQuery.bindValue(":market", market);
        destQuery.bindValue(":product", product);
        destQuery.bindValue(":data", table);
        destQuery.bindValue(":dataid", subTable);
        destQuery.bindValue(":timestamp", ttime);
        auto result = destQuery.exec();
        if(!result)
        {
            qDebug() << destQuery.lastError().text();
            ui->msg->append(QString("%1:%2").arg(selector).arg(name));
            qApp->processEvents();
            continue;
        }
        destQuery.exec("SELECT last_insert_rowid();");
        destQuery.next();
        auto id = destQuery.value(0).toInt();
        Q_ASSERT(id >= 1);

        int rowCount = 0;
        switch(selector)
        {
        case 1:
            rowCount = decode(value, destQuery, table, id);
            break;
        case 2:
            rowCount = decode2(value, destQuery, table, id);
            break;
        default:
            break;
        }

        destQuery.prepare("UPDATE datas SET datacoutn = :rowCount WHERE id = :id");
        destQuery.bindValue(":rowCount", rowCount);
        destQuery.bindValue(":id", id);
        destQuery.exec();

        ui->msg->append(QString("%1:%2:%3").arg(selector).arg(name).arg(rowCount));
        qApp->processEvents();
    }
}
