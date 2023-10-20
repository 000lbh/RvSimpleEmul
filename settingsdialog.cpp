#include "settingsdialog.h"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

const std::string &SettingsDialog::get_arguments()
{
    return arguments;
}

void SettingsDialog::accept()
{
    arguments = ui->argsEdit->text().toStdString();
    QDialog::accept();
}

void SettingsDialog::showEvent(QShowEvent *event)
{
    ui->argsEdit->setText(arguments.c_str());
    QDialog::showEvent(event);
}
