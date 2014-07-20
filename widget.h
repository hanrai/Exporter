#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QtSql>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

public slots:
    void on_dataFileButton_clicked();
    void on_targetButton_clicked();
    void on_startButton_clicked();

private:
    Ui::Widget *ui;

private:
    QSqlDatabase database;
    QSqlDatabase destDatabase;

private:
    bool OpenDatabase(QSqlDatabase &database, QString name, QString dbName);
    void InitInfoTable(QSqlQuery &query);
    void Decode(QSqlQuery &query, QSqlQuery &destQuery, int selector);
};

#pragma pack(1)
struct Header
{
    char bitOrder;
    int blocks;
    int datetime1;
    int datetime2;
    int count;
    int data;
};
#pragma pack()

void DecodeHeader(struct Header *header);

#endif // WIDGET_H
