# RvSimpleEmul

ָ�ģ����

## Introduction

��Ϊ��ϵ�ṹʵϰ��һ���֣���ģ����ʵ����RV64I��RV64M�д󲿷��з���ָ��Ͳ����޷���ָ���ģ�⡣

����������������ԣ�

- �Զ�����ģʽ��ͨ�������в����趨���ص㣬�Զ�����������������ڴ�״̬��
- ��������ģʽ���������öϵ㣬�������еȣ����Բ鿴�Ĵ��������Բ鿴�ڴ棬���Բ鿴ջ���ݡ�
- �������ݣ�ͨ�������в��������Խ����ֲ�����argv����ʽ���ݸ�main������

��������ϸ�����£�

- ELF���أ�ʹ��ELFIO�⣬���ݼ������к�LOAD��ǩ���ݵ���Ӧλ�ã��ڴ����4K���롣�Զ���ȡmain�����Լ�gp�Ĵ����ĵ�ַ������
- �ڴ�ģ�ͣ�ʹ��һ���򵥵�ҳ���Լ�Ȩ�޹�������operator[]ʵ���ڴ���ʣ����ذ�װ��������operator T��operator=ʵ���ڴ���ʿ��ơ�
- ������ģ�ͣ���ָ�����������Ϊ��ͨ���쳣����������ô桢��ת���Ƿ�ָ�Υ����ʵ�����
- 128λ�˷���ʵ�֣�gcc�ṩ��__int128_t���ͣ���msvc���°���_Signed128���ͣ���������Щ��ʵ�ֳ˷�ģ�⡣

## Usage

���벢���ԣ�
```
mkdir build
cd build
cmake ..
make
make test
```

��ȷ�����ϵͳ����sh�ű���������grep�������Windows�����Ƽ�ʹ��msys2��������ӵ�����������Linux/MacOS��Ӧ���������������

����11�����ԣ����Ƿ���ֵ���������á�ȫ�ֱ������ݹ���á�����ϵͳ��ȡ�ֻҪ������ϵͳ���õģ�������������������

## Known issues

�Զ�����ģʽ��ʱ�������ڴ棬�Ӷ������ڴ˹۲��ڴ档�����ڽ���ģʽ�۲��ڴ档�޷�ģ��ϵͳ���á�

## License

GPLv3 license is applied to all sources written by me. Licenses for 3rd party librarys, please refer to next section

## 3rd party library
- cxxopts, MIT License, https://github.com/jarro2783/cxxopts
- ELFIO, MIT License, https://github.com/serge1/ELFIO