#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>

#include "3rd/cxxopts.hpp"
#include "3rd/elfio/elfio.hpp"

#include "RvCpu.h"
#include "RvMem.h"
#include "RvInst.h"
#include "RvExcept.hpp"

constexpr uint64_t PGSIZE = 1 << 12;
constexpr uint64_t HALT_MAGIC = 0xdeadbeefdeadbeef;

int main(int argc, const char *argv[])
{
    cxxopts::Options options(argv[0], "Simple Risc-V Emulator");
    options.add_options()
        ("R,run", "Instantly run and return (default)")
        ("O,output", "Output to a file, default to stdout(-)", cxxopts::value<std::string>()->default_value("-"))
        ("M,memory", "Output to a file, default no file is generated", cxxopts::value<std::string>())
        ("B,address", "Set base address to ADDR(hex)", cxxopts::value<std::string>()->default_value("0"))
        ("I,interactive", "Interactive mode")
        ("A,arguments", "Arguments to be passed", cxxopts::value<std::string>()->default_value(""))
        ("h,help", "Display this content")
        ("FILE", "ELF file", cxxopts::value<std::string>())
    ;
    options.parse_positional({"FILE"});
    auto result{ options.parse(argc, argv) };
    if (result.count("run") && result.count("interactive")) {
        std::cerr << "Error: cannot specify --run with --interactive" << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }
    if (result.count("help")) {
        std::cerr << options.help() << std::endl;
        return 0;
    }
    uint64_t addr_base = std::stoull(result["address"].as<std::string>(), 0, 16);
    ELFIO::elfio reader;
    if (!result.count("FILE") || !reader.load(result["FILE"].as<std::string>())) {
        std::cerr << "No file specified or cannot open the file" << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }
    if (reader.get_class() != ELFIO::ELFCLASS64) {
        std::cerr << "ELF class error" << std::endl;
        return 1;
    }
    if (reader.get_encoding() != ELFIO::ELFDATA2LSB) {
        std::cerr << "ELF encoding error" << std::endl;
        return 1;
    }
    if (reader.get_machine() != ELFIO::EM_RISCV) {
        std::cerr << "ELF architecture error" << std::endl;
        std::cerr << "Expect Risc-V, found " << reader.get_machine() << std::endl;
        return 1;
    }
    if (reader.get_type() != ELFIO::ET_EXEC) {
        std::cerr << "ELF type error" << std::endl;
        std::cerr << "Expect EXEC, found " << reader.get_type() << std::endl;
        std::cerr << "Note: can only load static-link ELF files" << std::endl;
        return 1; 
    }
    RvMem mem;
    std::vector<std::unique_ptr<char []>> mem_segs;
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
            perm |= mem.P_READ;
        if (segment->get_flags() & ELFIO::PF_W)
            perm |= mem.P_WRITE;
        if (segment->get_flags() & ELFIO::PF_X)
            perm |= mem.P_EXEC;
        std::unique_ptr<char []> seg_mem{new char[end_vaddr - start_vaddr]{}};
        ::memcpy(&seg_mem[offset], segment->get_data(), fsize);
        for (auto i{start_vaddr + addr_base}; i < end_vaddr + addr_base; i += PGSIZE) {
            mem.map_page(i, perm, &seg_mem[i - start_vaddr - addr_base]);
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
        std::cerr << "Cannot find main symbol" << std::endl;
        return 1;
    }
    // Create stack, allocate a page
    std::unique_ptr<char[]> stack_ptr{ new char[PGSIZE] {} };
    constexpr uint64_t STACK_LIMIT = 0x80000000;
    constexpr uint64_t ARG_BASE = 0xB0000000;
    constexpr uint64_t PARG_BASE = 0xA0000000;
    mem.map_page(STACK_LIMIT - PGSIZE, mem.P_READ | mem.P_WRITE, stack_ptr.get());
    mem_segs.push_back(std::move(stack_ptr));
    RvReg reg;
    reg.ra = HALT_MAGIC;
    reg.sp = STACK_LIMIT - 8;
    reg.pc = main_addr.value().first + addr_base;
    reg.gp = global_ptr;
    // Pass arguments
    auto &&pargs_r{ result["arguments"].as<std::string>() };
    std::vector<std::string> pargs{ result["FILE"].as<std::string>() };
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
        mem.map_page(i, mem.P_READ, &ptr_args[i - ARG_BASE]);
    }
    std::unique_ptr<char[]> ptr_pargs{ new char[PGSIZE] {} };
    ::memcpy(ptr_pargs.get(), ppargs.data(), ppargs.size() * sizeof(uint64_t));
    mem.map_page(PARG_BASE, mem.P_READ, ptr_pargs.get());
    reg.a0 = pargc;
    reg.a1 = PARG_BASE;
    mem_segs.push_back(std::move(ptr_args));
    mem_segs.push_back(std::move(ptr_pargs));
    RvPipelineCpu cpu(mem, reg, std::make_shared<RvStaticBranchPred<false>>());
    cpu.add_breakpoint(HALT_MAGIC);
    // Interactive section
    if (result.count("interactive")) {
        std::string command;
        while (std::getline(std::cin, command)) {
            std::istringstream isin(command);
            std::string main_command;
            if (!(isin >> main_command))
                continue;
            if (main_command == "run" || main_command == "r") {
                uint64_t count{};
                isin >> count;
                auto result{ cpu.exec(count) };
                std::cout << std::dec << result << " instructions executed" << std::endl;
            }
            else if (main_command == "step" || main_command == "s") {
                cpu.exec(1, true);
                /////
                std::cout << "Pipeline status:" << std::endl;
                auto &&[f_i, d_i, e_i, m_i, w_i, f_c, d_c, e_c, m_c, w_c] { cpu.get_internal_status() };
                std::cout << std::format("  Fetch:       {:3} cycle(s), {}", f_c, f_i) << std::endl;
                std::cout << std::format("  Decode:      {:3} cycle(s), {}", d_c, d_i) << std::endl;
                std::cout << std::format("  Execute:     {:3} cycle(s), {}", e_c, e_i) << std::endl;
                std::cout << std::format("  Memory:      {:3} cycle(s), {}", m_c, m_i) << std::endl;
                std::cout << std::format("  Write-back:  {:3} cycle(s), {}", w_c, w_i) << std::endl;
                /////
            }
            else if (main_command == "info") {
                std::string sub_command;
                isin >> sub_command;
                if (sub_command == "regs") {
                    for (int i{ 0 }; i < 32; i++) {
                        std::cout << RVREGABINAME[i] << "=0x" << std::hex << static_cast<uint64_t>(cpu.reg[i]) << std::endl;
                    }
                    std::cout << "pc=0x" << std::hex << cpu.reg.pc << std::endl;
                }
                else if (sub_command == "stack") {
                    try {
                        uint64_t fp{ cpu.reg.fp };
                        while (fp != 0) {
                            std::cout << std::hex << static_cast<uint64_t>(cpu.mem[fp - 8]) << std::endl;
                            fp = static_cast<uint64_t>(cpu.mem[fp - 16]);
                        }
                    }
                    catch (const RvAccVio &) {
                        ;
                    }
                }
                else if (sub_command == "stat") {
                    std::cout << "Statistics:" << std::endl;
                    std::cout << "  Cycle count: " << std::dec << cpu.get_cycle_count() << std::endl;
                    std::cout << "  CPI: " << cpu.get_cpi() << std::endl;
                    std::cout << "  Instruction count:" << std::endl;
                    for (auto &[key, value] : cpu.get_inst_stat()) {
                        std::cout << "    " << key << ": " << value << std::endl;
                    }
                }
                else if (sub_command == "pipeline") {
                    std::cout << "Pipeline status:" << std::endl;
                    auto &&[f_i, d_i, e_i, m_i, w_i, f_c, d_c, e_c, m_c, w_c] { cpu.get_internal_status() };
                    std::cout << std::format("  Fetch:       {:3} cycle(s), {}", f_c, f_i) << std::endl;
                    std::cout << std::format("  Decode:      {:3} cycle(s), {}", d_c, d_i) << std::endl;
                    std::cout << std::format("  Execute:     {:3} cycle(s), {}", e_c, e_i) << std::endl;
                    std::cout << std::format("  Memory:      {:3} cycle(s), {}", m_c, m_i) << std::endl;
                    std::cout << std::format("  Write-back:  {:3} cycle(s), {}", w_c, w_i) << std::endl;
                }
                else {
                    std::cout << "Provide more arguments." << std::endl;
                    continue;
                }
            }
            else if (main_command == "x" || main_command == "examine") {
                uint64_t addr{};
                size_t len{};
                isin >> addr >> len;
                if (!len) {
                    std::cout << "Invalid arguments." << std::endl;
                    continue;
                }
                try {
                    for (size_t i{}; i < len; i++, addr++) {
                        uint8_t data{ static_cast<uint8_t>(cpu.mem[addr]) };
                        std::cout << std::hex << static_cast<uint64_t>(data) << " ";
                    }
                }
                catch (const RvAccVio &) {
                    std::cout << "Cannot access memory at 0x" << std::hex << addr;
                }
                std::cout << std::endl;
            }
            else if (main_command == "b" || main_command == "break") {
                uint64_t addr{};
                if (!(isin >> addr)) {
                    std::cout << "Invalid argument." << std::endl;
                    continue;
                }
                if (!cpu.add_breakpoint(addr)) {
                    std::cout << "Add breakpoint failed." << std::endl;
                }
            }
            else if (main_command == "d" || main_command == "delete") {
                uint64_t addr{};
                if (!(isin >> addr)) {
                    std::cout << "Invalid argument." << std::endl;
                    continue;
                }
                if (!cpu.remove_breakpoint(addr)) {
                    std::cout << "Delete breakpoint failed." << std::endl;
                }
            }
            else if (main_command == "disassemble" || main_command == "disas") {
                uint64_t addr{};
                if (!(isin >> addr)) {
                    addr = cpu.reg.pc;
                }
                try {
                    std::unique_ptr<RvInst> inst{ RvInst::decode(cpu.mem.fetch(addr)) };
                    std::cout << inst->name() << std::endl;
                }
                catch (const RvAccVio &) {
                    std::cout << "Cannot access memory at 0x" << std::hex << addr << std::endl;
                }
            }
            else if (main_command == "help" || main_command == "h") {
                std::cout << "Usage:" << std::endl;
                std::cout << "h,help                      Show this content" << std::endl;
                std::cout << "run,r [n=0]                 Run n instructions, infinite if n equals to 0" << std::endl;
                std::cout << "step,s                      Step one. Won't be affected by breakpoints" << std::endl;
                std::cout << "info regs|stack             Get register info or backtrace info" << std::endl;
                std::cout << "examine,x addr len          Examine memory content from addr with len bytes" << std::endl;
                std::cout << "break,b addr                Set breakpoint at addr" << std::endl;
                std::cout << "delete,d addr               Remove breakpoint ad addr" << std::endl;
                std::cout << "disassemble,disas [addr=pc] Disassemble at addr" << std::endl;
                std::cout << "quit,q                      Quit" << std::endl;
            }
            else if (main_command == "quit" || main_command == "q") {
                break;
            }
            else {
                std::cout << "Unknown command." << std::endl;
            }
        }
        return 0;
    }
    auto exec_result{ cpu.exec() };
    std::cout << "Processor exit after executed " << std::dec << exec_result << " instructions." << std::endl;
    std::cout << "Register status: " << std::endl;
    for (int i{0}; i < 32; i++) {
        std::cout << RVREGABINAME[i] << "=0x" << std::hex << static_cast<uint64_t>(cpu.reg[i]) << std::endl;
    }
    std::cout << "pc=0x" << std::hex << cpu.reg.pc << std::endl;
    std::cout << "Statistics:" << std::endl;
    std::cout << "  Cycle count: " << std::dec << cpu.get_cycle_count() << std::endl;
    std::cout << "  CPI: " << cpu.get_cpi() << std::endl;
    std::cout << "  Branch: " << cpu.get_branch_count() << std::endl;
    std::cout << "  Branch miss: " << cpu.get_branch_miss() << std::endl;
    std::cout << "  Miss rate: " << cpu.get_missrate() << std::endl;
    std::cout << "  Instruction count:" << std::endl;
    for (auto &[key, value] : cpu.get_inst_stat()) {
        std::cout << "    " << key << ": " << value << std::endl;
    }

    std::cout << "Pipeline status:" << std::endl;
    auto &&[f_i, d_i, e_i, m_i, w_i, f_c, d_c, e_c, m_c, w_c] { cpu.get_internal_status() };
    std::cout << std::format("  Fetch:       {:3} cycle(s), {}", f_c, f_i) << std::endl;
    std::cout << std::format("  Decode:      {:3} cycle(s), {}", d_c, d_i) << std::endl;
    std::cout << std::format("  Execute:     {:3} cycle(s), {}", e_c, e_i) << std::endl;
    std::cout << std::format("  Memory:      {:3} cycle(s), {}", m_c, m_i) << std::endl;
    std::cout << std::format("  Write-back:  {:3} cycle(s), {}", w_c, w_i) << std::endl;

    return 0;
}
