#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <string>
#include <format>

#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>

#include "3rd/elfio/elfio.hpp"

constexpr uint64_t PGSIZE = 1 << 12;
constexpr uint64_t ARG_BASE = 0xB0000000;
constexpr uint64_t PARG_BASE = 0xA0000000;
constexpr uint64_t HALT_MAGIC = 0xdeadbeefdeadbeef;

class InstWidgetItem final : public QListWidgetItem {
    uint64_t pc;
public:
    InstWidgetItem(const std::string &text, uint64_t pc, bool curr = false, bool bp = false)
        : pc{pc}
    {
        setText(QString(std::format("{}{:016x}       {}", bp ? '*' : ' ', pc, text).c_str()));
        if (curr) {
            setBackground(QColor(0, 0, 0));
            setForeground(QColor(255, 255, 255));
        }
        if (bp) {
            setForeground(QColor(255, 0, 0));
        }
    }
    uint64_t get_pc()
    {
        return pc;
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cpu{}
    , mem{}
    , branch_predictor{new RvStaticBranchPred<false>}
    , last_mem_addr{}
    , setDlg{new SettingsDialog(this)}
    , licDlg{new LicenseDialog(this)}
    , aboutDlg{new AboutDialog(this)}
{
    ui->setupUi(this);
    ui->toolBar->addWidget(ui->toolBarWidget);
    this->setCentralWidget(ui->centralWidget);
    runTimer.setInterval(100);
    runTimer.callOnTimeout([&, this] {
        if (!isRun && runner.joinable())
            emit sigCpuPause();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    auto fn{ QFileDialog::getOpenFileName(this, "Select ELF file...").toStdString() };
    if (fn != "")
        file_name.reset(new std::string(std::move(fn)));
    reload();
}

void MainWindow::reload()
{
    mem_segs.clear();
    if (!file_name) {
        ui->statusbar->showMessage(tr("Please open first."));
        return;
    }
    ELFIO::elfio reader;
    if (!reader.load(*file_name)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file"));
        ui->statusbar->showMessage(tr("ELF Load Error."));
        file_name.reset();
        return;
    }
    if (reader.get_class() != ELFIO::ELFCLASS64) {
        QMessageBox::critical(this, tr("Error"), tr("ELF class error"));
        ui->statusbar->showMessage(tr("ELF Load Error."));
        file_name.reset();
        return;
    }
    if (reader.get_encoding() != ELFIO::ELFDATA2LSB) {
        QMessageBox::critical(this, tr("Error"), tr("ELF encoding error"));
        ui->statusbar->showMessage(tr("ELF Load Error."));
        file_name.reset();
        return;
    }
    if (reader.get_machine() != ELFIO::EM_RISCV) {
        QMessageBox::critical(this, tr("Error"), tr("ELF architecture error"));
        ui->statusbar->showMessage(tr("ELF Load Error."));
        file_name.reset();
        return;
    }
    if (reader.get_type() != ELFIO::ET_EXEC) {
        QMessageBox::critical(this, tr("Error"), tr("ELF type error"));
        ui->statusbar->showMessage(tr("ELF Load Error. Note: Only support static-linked ELF"));
        file_name.reset();
        return;
    }
    // TODO: change addr_base to read settings
    uint64_t addr_base = 0x00000000;
    mem.reset(new RvMem);
    std::optional<std::pair<uint64_t, uint64_t>> main_addr{};
    uint64_t global_ptr{};
    for (auto &segment : reader.segments) {
        if (segment->get_type() != ELFIO::PT_LOAD)
            continue;
        auto fsize{segment->get_file_size()};
        auto msize{segment->get_memory_size()};
        auto align{segment->get_align()};
        auto start_vaddr{segment->get_virtual_address() & ~(align - 1)};
        auto end_vaddr{(segment->get_virtual_address() + msize + align - 1) & ~(align - 1)};
        auto offset{segment->get_virtual_address() - start_vaddr};
        int perm{};
        if (segment->get_flags() & ELFIO::PF_R)
            perm |= mem->P_READ;
        if (segment->get_flags() & ELFIO::PF_W)
            perm |= mem->P_WRITE;
        if (segment->get_flags() & ELFIO::PF_X)
            perm |= mem->P_EXEC;
        std::unique_ptr<char []> seg_mem{new char[end_vaddr - start_vaddr]{}};
        ::memcpy(&seg_mem[offset], segment->get_data(), fsize);
        for (auto i{start_vaddr + addr_base}; i < end_vaddr + addr_base; i += PGSIZE) {
            mem->map_page(i, perm, &seg_mem[i - start_vaddr - addr_base]);
        }
        mem_segs.push_back(std::move(seg_mem));
    }
    for (auto &section : reader.sections) {
        if (section->get_type() == ELFIO::SHT_SYMTAB) {
            const ELFIO::symbol_section_accessor symbols(reader, section.get());
            for (ELFIO::Elf_Xword i = 0; i < symbols.get_symbols_num(); i++) {
                std::string name;
                ELFIO::Elf64_Addr addr;
                ELFIO::Elf_Xword size;
                unsigned char bind;
                unsigned char type;
                ELFIO::Elf_Half sec_index;
                unsigned char other;
                symbols.get_symbol(i, name, addr, size, bind, type, sec_index, other);
                // Get main location
                if (name == "main") {
                    main_addr = {addr, size};
                }
                // Get gp register value
                if (name == "__global_pointer$" || name == "_gp") {
                    global_ptr = addr;
                }
            }
        }
    }
    if (!main_addr) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot find main symbol"));
        file_name.reset();
        return;
    }
    if (!global_ptr) {
        ui->statusbar->showMessage(tr("Warning: Global Pointer(gp) not Found."));
    }
    // Create stack, allocate a page
    std::unique_ptr<char[]> stack_ptr{ new char[PGSIZE] {} };
    constexpr uint64_t STACK_LIMIT = 0x80000000;
    constexpr uint64_t ARG_BASE = 0xB0000000;
    constexpr uint64_t PARG_BASE = 0xA0000000;
    mem->map_page(STACK_LIMIT - PGSIZE, mem->P_READ | mem->P_WRITE, stack_ptr.get());
    mem_segs.push_back(std::move(stack_ptr));
    RvReg reg;
    reg.ra = HALT_MAGIC;
    reg.sp = STACK_LIMIT - 8;
    reg.pc = main_addr.value().first + addr_base;
    reg.gp = global_ptr;
    // Pass arguments
    std::string pargs_r{ setDlg->get_arguments() };
    std::vector<std::string> pargs{ *file_name };
    std::vector<uint64_t> ppargs;
    ppargs.push_back(ARG_BASE);
    std::stringstream psin(pargs_r);
    std::string parg;
    size_t arg_size{ pargs[0].length() + 1 };
    while (psin >> parg) {
        ppargs.push_back(ARG_BASE + arg_size);
        arg_size += parg.length() + 1;
        pargs.push_back(parg);
    }
    arg_size = (arg_size + PGSIZE - 1) & ~(PGSIZE - 1);
    auto pargc{ ppargs.size() };
    std::unique_ptr<char[]> ptr_args{ new char[arg_size] {} };
    for (size_t i{ 0 }; i < pargs.size(); i++)
        ::memcpy(ptr_args.get() + ppargs[i] - ARG_BASE, pargs[i].c_str(), pargs[i].length() + 1);
    for (auto i{ ARG_BASE }; i < ARG_BASE + arg_size; i += PGSIZE) {
        mem->map_page(i, mem->P_READ, &ptr_args[i - ARG_BASE]);
    }
    std::unique_ptr<char[]> ptr_pargs{ new char[PGSIZE] {} };
    ::memcpy(ptr_pargs.get(), ppargs.data(), ppargs.size() * sizeof(uint64_t));
    mem->map_page(PARG_BASE, mem->P_READ, ptr_pargs.get());
    reg.a0 = pargc;
    reg.a1 = PARG_BASE;
    mem_segs.push_back(std::move(ptr_args));
    mem_segs.push_back(std::move(ptr_pargs));
    switch (setDlg->get_branch_predictor()) {
    case 0:
        branch_predictor.reset(new RvStaticBranchPred<false>);
        break;
    case 1:
        branch_predictor.reset(new RvStaticBranchPred<true>);
        break;
    case 2:
        branch_predictor.reset(new RvStaticBTFNTBranchPred);
        break;
    case 3:
        branch_predictor.reset(new RvSatCtrPred<2>(8));
        break;
    case 4:
        branch_predictor.reset(new RvSatCtrPred<2>(12));
        break;
    case 5:
        branch_predictor.reset(new RvSatCtrPred<3>(8));
        break;
    case 6:
        branch_predictor.reset(new RvSatCtrPred<3>(12));
        break;
    default:
        branch_predictor.reset(new RvStaticBranchPred<false>);
        break;
    }

    cpu.reset(new RvPipelineCpu(*mem, reg, branch_predictor));
    cpu->add_breakpoint(HALT_MAGIC);
    updateReg();
    updateInsts(cpu->reg.pc);
    updateMem(cpu->reg.gp);
    updatePipeline();
    resetStat();
    return;
}

void MainWindow::aboutPage()
{
    aboutDlg->show();
    return;
}

void MainWindow::licensePage()
{
    licDlg->show();
    return;
}

void MainWindow::settingsPage()
{
    setDlg->show();
    return;
}

void MainWindow::cpuRun()
{
    if (!cpu) {
        ui->statusbar->showMessage(tr("Load ELF first"));
        return;
    }
    ui->stepButton->setDisabled(true);
    ui->breakButton->setDisabled(true);
    ui->runButton->setDisabled(true);
    ui->pauseButton->setEnabled(true);

    isRun = true;
    runTimer.start();
    runner = std::thread([&, this] {
        uint64_t inst_exec{};
        try {
            for (;;) {
                if (this->cpu->find_breakpoint(cpu->reg.pc) && inst_exec)
                    break;
                if (this->shouldStop)
                    break;
                this->cpu->step();
                inst_exec++;
            }
        }
        catch (const RvHalt &e) {
            ;
        }
        catch (const RvException &e) {
            std::cerr << "We encountered an exception " << typeid(e).name() << ", " << e.what() << std::endl;
        }
        this->lastExecuted = inst_exec;
        isRun = false;
        return;
    });
}

void MainWindow::cpuPause()
{
    if (!cpu)
        return;
    runTimer.stop();
    shouldStop = true;
    runner.join();
    ui->statusbar->showMessage(QString::asprintf(tr("Executed %d cycles.").toStdString().c_str(), static_cast<uint64_t>(lastExecuted)));
    shouldStop = false;

    ui->stepButton->setEnabled(true);
    ui->breakButton->setEnabled(true);
    ui->runButton->setEnabled(true);
    ui->pauseButton->setDisabled(true);

    updateReg();
    updateInsts();
    updateMem();
    updatePipeline();
}

void MainWindow::cpuBreak()
{
    if (!cpu)
        return;
    auto instItem{ dynamic_cast<InstWidgetItem *>(ui->instListWidget->currentItem()) };
    if (instItem) {
        if (cpu->find_breakpoint(instItem->get_pc()))
            cpu->remove_breakpoint(instItem->get_pc());
        else
            cpu->add_breakpoint(instItem->get_pc());
        updateInsts();
    }
}

void MainWindow::cpuStep()
{
    if (!cpu)
        return;
    cpu->exec(1, true);
    updateReg();
    updateInsts();
    updateMem();
    updatePipeline();
}

void MainWindow::cpuReload()
{
    if (runner.joinable()) {
        shouldStop = true;
        runner.join();
    }
    shouldStop = false;
    isRun = false;

    ui->stepButton->setEnabled(true);
    ui->breakButton->setEnabled(true);
    ui->runButton->setEnabled(true);
    ui->pauseButton->setDisabled(true);

    reload();
}

void MainWindow::updateReg()
{
    ui->regListWidget->clear();
    if (ui->hexDisplayCheckBox->checkState() == Qt::Checked) {
        ui->regListWidget->addItem(QString(std::format("fetch_pc={:#016x}", cpu->get_fetch_pc()).c_str()));
    }
    else {
        ui->regListWidget->addItem(QString(std::format("fetch_pc={}", cpu->get_fetch_pc()).c_str()));
    }
    for (int i{}; i < 32; i++) {
        if (ui->hexDisplayCheckBox->checkState() == Qt::Checked) {
            ui->regListWidget->addItem(QString(std::format("{}={:#016x}", RVREGABINAME[i], cpu->reg.get(i)).c_str()));
        }
        else {
            ui->regListWidget->addItem(QString(std::format("{}={}", RVREGABINAME[i], cpu->reg.get(i)).c_str()));
        }
    }
}

void MainWindow::updateInsts()
{
    ui->instListWidget->clear();
    for (auto i{cpu->get_fetch_pc() - 16}; i < cpu->get_fetch_pc() + 32; i += 4) {
        std::string disas{};
        try {
            disas = std::unique_ptr<RvInst>(RvInst::decode(cpu->mem.fetch(i)))->name();
        }
        catch (const RvIllIns &) {
            disas = "Undefined";
        }
        catch (const RvAccVio &) {
            disas = "Memory cannot access";
        }
        catch (const RvMisAlign &) {
            disas = "PC not aligned";
        }

        ui->instListWidget->addItem(new InstWidgetItem(disas, i, i == cpu->get_fetch_pc(), cpu->find_breakpoint(i)));
    }
}

void MainWindow::updateInsts(uint64_t addr)
{
    ui->instListWidget->clear();
    for (auto i{addr - 16}; i < addr + 32; i += 4) {
        std::string disas{};
        try {
            disas = std::unique_ptr<RvInst>(RvInst::decode(cpu->mem.fetch(i)))->name();
        }
        catch (const RvIllIns &) {
            disas = "Undefined";
        }
        catch (const RvAccVio &) {
            disas = "Memory cannot access";
        }
        catch (const RvMisAlign &) {
            disas = "PC not aligned";
        }

        ui->instListWidget->addItem(new InstWidgetItem(disas, i, i == cpu->reg.pc, cpu->find_breakpoint(i)));
    }
}

void MainWindow::updateMem()
{
    ui->memListWidget->clear();
    for (auto i{last_mem_addr}; i < last_mem_addr + 16 * 16; i += 16) {
        std::string result = std::format("{:016x}     ", i);
        for (int j{}; j < 16; j++) {
            try {
                result = std::format("{} {:02x}", result, static_cast<uint8_t>(cpu->mem[i + j]));
            }
            catch (const RvAccVio &) {
                result += " XX";
            }
        }
        ui->memListWidget->addItem(QString(result.c_str()));
    }
    return;
}

void MainWindow::updateMem(uint64_t addr)
{
    last_mem_addr = addr & ~0xf;
    updateMem();
    return;
}

void MainWindow::updatePipeline()
{
    auto [f_i, d_i, e_i, m_i, w_i, f_c, d_c, e_c, m_c, w_c] = cpu->get_internal_status();
    ui->fetchCycle->setText(QString::number(f_c));
    ui->decodeCycle->setText(QString::number(d_c));
    ui->execCycle->setText(QString::number(e_c));
    ui->memCycle->setText(QString::number(m_c));
    ui->wbCycle->setText(QString::number(w_c));
    ui->fetchInst->setText(f_i.c_str());
    ui->decodeInst->setText(d_i.c_str());
    ui->execInst->setText(e_i.c_str());
    ui->memInst->setText(m_i.c_str());
    ui->wbInst->setText(w_i.c_str());
    return;
}

void MainWindow::memJump()
{
    bool ok;
    uint64_t result{ui->memTargetEdit->text().toULongLong(&ok, 16)};
    if (!ok) {
        ui->statusbar->showMessage(tr("Cannot jump to this address"));
        return;
    }
    updateMem(result);
}

void MainWindow::showStat()
{
    if (!cpu)
        return;
    uint64_t cycleCnt = cpu->get_cycle_count();
    uint64_t instCnt = cpu->get_inst_count();
    double CPI = cpu->get_cpi();
    uint64_t branchCnt = cpu->get_branch_count();
    uint64_t missCnt = cpu->get_branch_miss();
    double missRate = cpu->get_missrate();
    uint64_t squashedCnt = cpu->get_squashed_inst_count();
    uint64_t rawStallCnt = cpu->get_raw_stall_count();
    QString message{};
    std::string a = std::format("Cycle: {}\nInstructions: {}\nCPI: {}\nBranch: {}\nMiss: {}\nMissRate: {}\nSquashed: {}\nData hazard stall: {}\n", cycleCnt, instCnt, CPI, branchCnt, missCnt, missRate, squashedCnt, rawStallCnt);
    message += a.c_str();
    a = "Instructions:\n";
    for (const auto &[key, value] : cpu->get_inst_stat()) {
        a += std::format("  {:6s}: {}\n", key, value);
    }
    message += a.c_str();
    QMessageBox::information(this, "Statistics", message);
}

void MainWindow::resetStat()
{
    if (!cpu)
        return;
    cpu->reset_stat();
}

