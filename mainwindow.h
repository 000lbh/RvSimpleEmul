#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <cstdint>

#include <QMainWindow>
#include <QTimer>

#include "RvCpu.h"
#include "RvInst.h"
#include "RvMem.h"
#include "RvExcept.hpp"

#include "settingsdialog.h"
#include "aboutdialog.h"
#include "licensedialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void openFile();
    void aboutPage();
    void licensePage();
    void settingsPage();
    void updateReg();
    void updateInsts();
    void updateInsts(uint64_t addr);
    void updateMem();
    void updateMem(uint64_t addr);
    void cpuRun();
    void cpuPause();
    void cpuStep();
    void cpuReload();
    void cpuBreak();
    void reload();
    void memJump();
    void showStat();
    void resetStat();

signals:
    void sigCpuPause();

private:
    Ui::MainWindow *ui;
    std::unique_ptr<SettingsDialog> setDlg;
    std::unique_ptr<LicenseDialog> licDlg;
    std::unique_ptr<AboutDialog> aboutDlg;
    QTimer runTimer;
    std::atomic<bool> isRun;
    std::atomic<bool> shouldStop;
    std::thread runner;
    std::atomic<uint64_t> lastExecuted;
    std::unique_ptr<RvMultiCycleCpu> cpu;
    std::unique_ptr<RvMem> mem;
    std::unique_ptr<std::string> file_name;
    std::vector<std::unique_ptr<char[]>> mem_segs;
    uint64_t last_mem_addr;
};
#endif // MAINWINDOW_H
