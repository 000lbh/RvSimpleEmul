#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include <string>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    const std::string &get_arguments();
    int get_branch_predictor();

public slots:
    void accept() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::SettingsDialog *ui;

    std::string arguments;
};

#endif // SETTINGSDIALOG_H
