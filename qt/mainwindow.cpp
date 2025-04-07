#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_loadBook_clicked()
{
    ui->bookRender->loadBook(ui->dirPath->text());
    ui->pageSpinBox->setMaximum(ui->bookRender->getPageCount());
}

void MainWindow::on_pageSpinBox_valueChanged(int arg1)
{
    ui->bookRender->setPage(arg1);
}


void MainWindow::on_rightToLeft_clicked()
{
    ui->bookRender->set_right_to_left();
}


void MainWindow::on_horizontalSlider_sliderMoved(int position)
{
    ui->bookRender->set_soft_cover_z(float(position)/100);
}


void MainWindow::on_checkBox_stateChanged(int arg1)
{
    ui->bookRender->set_hide_hyousiura(arg1);
}


void MainWindow::on_checkBox_2_stateChanged(int arg1)
{
    ui->bookRender->set_hide_soft_cover(arg1);
}

