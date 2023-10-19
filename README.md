# RvSimpleEmul

指令级模拟器

## Introduction

作为体系结构实习的一部分，本模拟器实现了RV64I和RV64M中大部分有符号指令和部分无符号指令的模拟。

本程序具有以下特性：

- 自动运行模式：通过命令行参数设定加载点，自动运行至结束，输出内存状态。
- 交互运行模式：可以设置断点，单步运行等，可以查看寄存器，可以查看内存，可以查看栈回溯。
- 参数传递：通过命令行参数，可以将部分参数以argv的形式传递给main函数。

本程序技术细节如下：

- ELF加载：使用ELFIO库，根据加载所有含LOAD标签数据到对应位置，内存分配4K对齐。自动获取main函数以及gp寄存器的地址并加载
- 内存模型：使用一个简单的页表以及权限管理，重载operator[]实现内存访问，返回包装对象并重载operator T和operator=实现内存访问控制。
- 处理器模型：由指令对象定义其行为，通过异常控制流解决访存、跳转、非法指令、违规访问等问题
- 128位乘法的实现：gcc提供了__int128_t类型，而msvc最新版有_Signed128类型，我们用这些来实现乘法模拟。

## Usage

编译并测试：
```
mkdir build
cd build
cmake ..
make
make test
```

请确保你的系统含有sh脚本解析器和grep命令。对于Windows，我推荐使用msys2并将其添加到环境变量。Linux/MacOS下应该无需特殊操作。

共计11个测试，涵盖返回值、参数设置、全局变量、递归调用、部分系统库等。只要不含有系统调用的，基本可以正常工作。

## Known issues

自动运行模式暂时不导出内存，从而不能在此观察内存。可以在交互模式观察内存。无法模拟系统调用。

## License

GPLv3 license is applied to all sources written by me. Licenses for 3rd party librarys, please refer to next section

## 3rd party library
- cxxopts, MIT License, https://github.com/jarro2783/cxxopts
- ELFIO, MIT License, https://github.com/serge1/ELFIO