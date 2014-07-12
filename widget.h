#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

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
};

#endif // WIDGET_H
