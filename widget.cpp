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
    QCoreApplication::setOrganizationName("hanrai");
    QCoreApplication::setOrganizationDomain("hanrai.com");
    QCoreApplication::setApplicationName("Exporter");
    QSettings settings;
    ui->dataFile->setText(settings.value("filenames/src", QString()).toString());
    ui->targetFolder->setText(settings.value("filenames/dest", QString()).toString());
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_dataFileButton_clicked()
{
    QString path = "E:/";
    if(!ui->dataFile->text().isEmpty())
        path = QDir(ui->dataFile->text()).absolutePath();
    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Open Data File"),
                                                    path,
                                                    tr("Data File(data.dat)"));
    if(filename.isEmpty())
        return;
    ui->dataFile->setText(filename);
    QSettings settings;
    settings.setValue("filenames/src", filename);
}

void Widget::on_targetButton_clicked()
{
    QString path = "E:/";
    if(!ui->targetFolder->text().isEmpty())
        path = QDir(ui->targetFolder->text()).absolutePath();
    QString filename = QFileDialog::getSaveFileName(
                this,
                tr("Export to..."),
                path,
                tr("SQLite (*.sqlite)"));
    if(filename.isEmpty())
        return;
    ui->targetFolder->setText(filename);
    QSettings settings;
    settings.setValue("filenames/dest", filename);
}

int decode(QByteArray &data,
           QSqlQuery &query,
           QString &table,
           int id)
{
    if(data.size() <= 17)
        return 0;
    Header *header = (Header*)data.data();
    DecodeHeader(header);

    auto count = header->count;
    if(!count)
        return count;

    query.exec("CREATE TABLE IF NOT EXISTS t_" + table + ""
               "(id INTEGER NOT NULL REFERENCES datas(id) ON DELETE CASCADE, "
               "timestamp INTEGER NOT NULL, value1 REAL NOT NULL)");
    query.exec("CREATE INDEX IF NOT EXISTS iid_" + table + " ON t_" + table + "(id);");

    query.prepare("INSERT INTO t_" + table + " VALUES "
                  "(:id, :timestamp, :data)");

    quint8 *cursor = (quint8*)&header->data;
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
        if(!query.exec())
            qDebug()<<query.lastError().text();
    }
    return count;
}

int decode2(QByteArray &data,
            QSqlQuery &query,
            QString &table,
            int id)
{
    if(data.size()<=17)
        return 0;
    Header *header = (Header*)data.data();
    DecodeHeader(header);

    auto count = header->count;
    if(!count)
        return count;

    quint8 *cursor = (quint8*)&header->data;
    auto cloneCursor = cursor;
    cloneCursor += 4;
    auto colCount = ntohl(*(quint32*)cloneCursor);

    QString queryString = "CREATE TABLE IF NOT EXISTS t_" + table + ""
            "(id INTEGER NOT NULL REFERENCES datas(id) ON DELETE CASCADE, "
            "timestamp INTEGER NOT NULL";
    for(int i=0; i<colCount; i++)
        queryString += QString(", value%1 REAL NOT NULL").arg(i+1);
    queryString += ");";
    query.exec(queryString);
    query.exec("CREATE INDEX IF NOT EXISTS iid_" + table + " ON t_" + table + "(id);");

    queryString = "INSERT INTO t_" + table + " VALUES "
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
            query.exec("DROP TABLE t_" + table + "");
            return -1;
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
        if(!query.exec())
            qDebug()<<query.lastError().text();
    }
    return count;
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
    QSqlQuery destQuery(destDatabase);
    destQuery.exec("PRAGMA foreign_keys = 1");
    destQuery.setForwardOnly(true);
    destDatabase.transaction();

    InitInfoTable(destQuery);

    QSqlQuery query(database);
    query.setForwardOnly(true);
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
        auto id = destQuery.lastInsertId().toInt();
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

        destQuery.prepare("UPDATE datas SET datacount = :rowCount WHERE id = :id");
        destQuery.bindValue(":rowCount", rowCount);
        destQuery.bindValue(":id", id);
        destQuery.exec();

        ui->msg->append(QString("%1:%2:%3").arg(selector).arg(name).arg(rowCount));
        qApp->processEvents();
    }
}

void DecodeHeader(struct Header *header)
{
    header->blocks = ntohl(*(int*)&header->blocks);
    header->count = ntohl(*(int*)&header->count);
}
