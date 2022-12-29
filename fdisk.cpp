#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <conio.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <windows.h>
using namespace std;

#define MEM_SIZE 1024 * 1024                         // 总磁盘空间为1M 1024*1024bytes 1byte=8bit
#define DISK_SIZE 1024                               // 磁盘块大小1K
#define DISK_NUM 1024                                // 一共有1k个磁盘块
#define MAX_FILE_SIZE 10240                          // 最大文件长度
#define MSD 7                                        // 最大子目录数max subdirect
#define FATSIZE DISK_NUM * sizeof(fatItem)           // FAT表大小（存储空间大小）
#define USER_ROOT_STARTBLOCK FATSIZE / DISK_SIZE + 1 // 用户根目录起始盘块号  一个Fat表占用几个磁盘块
#define USER_ROOT_SIZE sizeof(direct)                // 用户根目录大小 396

// FCB文件控制块
typedef struct FCB
{                         // 文件控制块 一共有44个bytes
    char fileName[20];    // 文件或目录名
    int type;             // 0--文件  1--目录
    char *content = NULL; // 文件内容
    int size;             // 大小
    int firstDisk;        // 起始盘块
    int next;             // 子目录起始盘块号
    int sign;             // 0--普通目录  1--根目录
    bool isOpened = false;
} FCB;

// 目录
struct direct
{
    FCB directItem[MSD + 2];
};

// 检查shell输入
struct Check
{
    int level;
    string s;
};

// 文件分配表i
typedef struct fatItem
{
    int item; // 存放文件下一个磁盘的指针
    int state;
} fatItem, *FAT;
// fatItem fatItem
// fatItem *FAT

string userLogin = "userfile.txt"; // 用户目录表，用于记录用户名和密码
string userName, userPassword;     // 当前用户名和密码
string userDat;                    // 用户磁盘数据
FAT fat;                           // FAT表
direct *root;                      // 根目录
direct *curDir;                    // 当前目录
string curPath;                    // 当前路径
char *fdisk;                       // 虚拟磁盘起始地址

// 字符串string转换为int类型
int ParseInt(string s)
{
    int l = s.length();
    int a[7];
    for (int i = 0; i < l; i++)
        a[i + 1] = s[i] - '0';
    int num = 0;
    for (int i = 1; i <= l; i++)
    {
        num *= 10;
        num += a[i];
    }
    return num;
}

// 去两边空格
void strip(string &str, char ch)
{
    int i = 0;
    while (str[i] == ch)
        i++;
    int j = str.size() - 1;
    while (str[j] == ch)
        j--;
    str = str.substr(i, j + 1 - i);
}

// 划分用户名和密码
void halfStr(string &userName, string &password, string line)
{
    int i;
    int len = line.length();
    for (i = 0; i < len; i++)
        if (line[i] == ' ')
            break;
    userName = line.substr(0, i);
    password = line.substr(i + 1, len);
}

void Shell()
{
    // 设置用户名和路径的前景色和背景色
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
    cout << curPath << ">"; // 输出用户名和路径
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    // 变回原来的颜色
}

/*保存磁盘数据*/
int save()
{
    ofstream fp;
    fp.open(userDat, ios::out | ios::binary);
    if (fp)
    {
        fp.write(fdisk, MEM_SIZE * sizeof(char)); // 将fdisk写入userDat文件
        fp.close();
        return 1;
    }
    else
    {
        cout << userDat << "打开失败！" << endl;
        fp.close();
        return 0;
    }
}

// 初始化磁盘结构
void init(string userDat)
{
    fdisk = new char[MEM_SIZE * sizeof(char)]; // 申请磁盘空间 1024*1024*char
    for (int i = 0; i < MEM_SIZE * sizeof(char); i++)
    {
        fdisk[i] = '+';
    }
    // 初始化FAT表
    fat = (FAT)(fdisk + DISK_SIZE); // fat地址 指向第一个磁盘块的地址
    fat[0].item = -1;               // 指针域
    fat[0].state = 1;               // 初始化FAT表
    for (int i = 1; i < USER_ROOT_STARTBLOCK; i++)
    { // <= ???
        fat[i].item = i + 1;
        fat[i].state = 1;
    } // 第1-8个磁盘用来存放 FAT表
    fat[USER_ROOT_STARTBLOCK].item = -1;
    fat[USER_ROOT_STARTBLOCK].state = 1; // 根目录磁盘块号
    // fat其他块初始化
    for (int i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
    {
        fat[i].item = -1;
        fat[i].state = 0;
    }
    // 根目录初始化
    root = (struct direct *)(fdisk + DISK_SIZE + FATSIZE); // 根目录地址
    root->directItem[0].sign = 1;
    root->directItem[0].firstDisk = USER_ROOT_STARTBLOCK; // 9
    strcpy(root->directItem[0].fileName, ".");            // 根目录名称为 .
    root->directItem[0].next = root->directItem[0].firstDisk;
    root->directItem[0].type = 1;
    root->directItem[0].size = USER_ROOT_SIZE;
    // 父目录初始化
    root->directItem[1].sign = 1;
    root->directItem[1].firstDisk = USER_ROOT_STARTBLOCK;
    strcpy(root->directItem[1].fileName, "..");
    root->directItem[1].next = root->directItem[0].firstDisk;
    root->directItem[1].type = 1;
    root->directItem[1].size = USER_ROOT_SIZE;
    // 子目录初始化
    for (int i = 2; i < MSD + 2; i++)
    {
        root->directItem[i].sign = 0;
        root->directItem[i].firstDisk = -1;
        strcpy(root->directItem[i].fileName, "");
        root->directItem[i].next = -1;
        root->directItem[i].type = 0;
        root->directItem[i].size = 0;
    }
    if (!save())
        cout << "初始化失败！" << endl;
}

/*读取用户磁盘数据内容*/
void readUserDisk(string userDat)
{
    fdisk = new char[MEM_SIZE]; // 申请磁盘大小缓冲区
    ifstream fp;
    fp.open(userDat, ios::in | ios::binary);
    if (fp)
        fp.read(fdisk, MEM_SIZE);
    else
        cout << userDat << "打开失败！" << endl;
    fp.close();
    fat = (FAT)(fdisk + DISK_SIZE);                        // fat地址
    root = (struct direct *)(fdisk + DISK_SIZE + FATSIZE); // 根目录地址
    curDir = root;
    curPath = userName + ":\\";
}

/*写文件内容*/
void write(string *str, char *content, int size)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 在当前目录下查找目标文件
    int i, j;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该文件！" << endl;
        return;
    }

    int cur = i;                                  // 当前目录项的下标
    int fSize = curDir->directItem[cur].size;     // 目标文件大小
    int item = curDir->directItem[cur].firstDisk; // 目标文件的起始磁盘块号
    while (fat[item].item != -1)
        item = fat[item].item;                                  // 计算保存该文件的最后一块盘块号
    char *first = fdisk + item * DISK_SIZE + fSize % DISK_SIZE; // 计算该文件的末地址
    // 如果盘块剩余部分够写，则直接写入剩余部分
    if (DISK_SIZE - fSize % DISK_SIZE > size)
    {
        strcpy(first, content);
        curDir->directItem[cur].size += size;
    }
    // 如果盘块剩余部分不够写，则找到空闲磁盘块写入
    else
    {
        // 先将起始磁盘剩余部分写完
        for (j = 0; j < DISK_SIZE - fSize % DISK_SIZE; j++)
        {
            first[j] = content[j];
        }
        int res_size = size - (DISK_SIZE - fSize % DISK_SIZE); // 剩余要写的内容大小
        int needDisk = res_size / DISK_SIZE;                   // 占据的磁盘块数量
        int needRes = res_size % DISK_SIZE;                    // 占据最后一块磁盘块的大小
        if (needDisk > 0)
            needRes += 1;
        for (j = 0; j < needDisk; j++)
        {
            for (i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
                if (fat[i].state == 0)
                    break;
            if (i >= DISK_NUM)
            {
                cout << "磁盘已被分配完！" << endl;
                return;
            }
            first = fdisk + i * DISK_SIZE; // 空闲磁盘起始盘物理地址
            // 当写到最后一块磁盘，则只写剩余部分内容
            if (j == needDisk - 1)
            {
                for (int k = 0; k < size - (DISK_SIZE - fSize % DISK_SIZE - j * DISK_SIZE); k++)
                    first[k] = content[k];
            }
            else
            {
                for (int k = 0; k < DISK_SIZE; k++)
                    first[k] = content[k];
            }
            // 修改文件分配表内容
            fat[item].item = i;
            fat[i].state = 1;
            fat[i].item = -1;
        }
        curDir->directItem[cur].size += size;
    }
}

// 显示文件内容
bool read(string *str, char *buf)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 检查该目录下是否有目标文件
    int i, j;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该文件！" << endl;
        return 0;
    }
    if (curDir->directItem[i].isOpened == false)
    {
        cout << "文件未打开，请先打开文件" << endl;
        return 0;
    }
    int item = curDir->directItem[i].firstDisk; // 目标文件的起始磁盘块号
    int fSize = curDir->directItem[i].size;     // 目标文件的大小
    int needDisk = fSize / DISK_SIZE;           // 占据的磁盘块数量
    int needRes = fSize % DISK_SIZE;            // 占据最后一块磁盘块的大小
    if (needRes > 0)
        needDisk += 1;
    char *first = fdisk + item * DISK_SIZE; // 起始磁盘块的物理地址
    // 将目录文件内容拷贝到缓冲区中
    if (fSize <= 0)
    {
        buf[0] = '\0';
        return 0;
    }
    for (i = 0; i < needDisk; i++)
    {
        for (j = 0; j < fSize - i * DISK_SIZE; j++)
        {
            buf[i * DISK_SIZE + j] = first[j];
        }
        if (i == needDisk - 1 && j == fSize - i * DISK_SIZE)
            buf[i * DISK_SIZE + j] = '\0';
        if (i != needDisk - 1)
        {
            item = fat[item].item;
            first = fdisk + item * DISK_SIZE;
        }
    }
    return 1;
}

/*touch创建文件*/
int touch(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str()); // c_str转换为字符串数组
    // 检查文件名是否合法
    if (!strcmp(fName, ""))
    {
        puts("创建失败!请输入文件名！");
        return 0;
    }
    if (str[1].length() > 20)
    {
        cout << "文件名太长！请重新输入！" << endl;
        return 0;
    }
    // 找当前目录下是否有文件重名
    for (int pos = 2; pos < MSD + 2; pos++)
    {
        if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0)
        {
            cout << "当前目录下已存在文件重名!" << endl;
            return 0;
        }
    }
    // 检查当前目录下空间是否已满
    int i;
    for (i = 2; i < MSD + 2; i++)
        if (curDir->directItem[i].firstDisk == -1)
            break;
    if (i >= MSD + 2)
    {
        cout << "当前目录下空间已满" << endl;
        return 0;
    }
    // 检查是否有空闲磁盘块
    int j;
    for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
        if (fat[j].state == 0)
        {
            fat[j].state = 1;
            break;
        }
    if (j >= DISK_NUM)
    {
        cout << "无空闲盘块！" << endl;
        return 0;
    }
    // 文件初始化
    curDir->directItem[i].sign = 0;
    curDir->directItem[i].firstDisk = j;
    strcpy(curDir->directItem[i].fileName, fName);
    curDir->directItem[i].next = j;
    curDir->directItem[i].type = 0;
    curDir->directItem[i].size = 0;
    curDir->directItem[i].isOpened = false;

    return 1;
}

/*显示当前目录下子目录和文件信息*/
void ls()
{
    cout << fixed << left << setw(20) << "文件名" << setw(10) << "类型" << setw(10) << "大小</B>" << setw(10) << "起始磁盘块号" << endl;
    for (int i = 2; i < MSD + 2; i++)
    {
        if (curDir->directItem[i].firstDisk != -1)
        {
            cout << fixed << setw(20) << curDir->directItem[i].fileName;
            if (curDir->directItem[i].type == 0)
            { // 显示文件
                cout << fixed << setw(10) << " " << setw(10) << curDir->directItem[i].size;
            }
            else
            { // 显示目录
                cout << fixed << setw(10) << "<DIR>" << setw(10) << " ";
            }
            cout << setw(10) << curDir->directItem[i].firstDisk << endl;
        }
    }
}

/*创建目录*/
int mkdir(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 检查目录名是否合法
    if (!strcmp(fName, ""))
        return 0;
    if (str[1].length() > 10)
    {
        cout << "目录名太长！请重新输入！" << endl;
        return 0;
    }
    if (!strcmp(fName, ".") || !strcmp(fName, ".."))
    {
        cout << "目录名称不合法！" << endl;
        return 0;
    }
    // 找当前目录下是否有目录重名
    for (int pos = 2; pos < MSD + 2; pos++)
    {
        if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 1)
        {
            cout << "当前目录下已存在目录重名!" << endl;
            return 0;
        }
    }
    // 检查当前目录下空间是否已满
    int i;
    for (i = 2; i < MSD + 2; i++)
        if (curDir->directItem[i].firstDisk == -1)
            break;
    if (i >= MSD + 2)
    {
        cout << "当前目录下空间已满" << endl;
        return 0;
    }
    // 检查是否有空闲磁盘块
    int j;
    for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
        if (fat[j].state == 0)
        {
            fat[j].state = 1;
            break;
        }
    if (j >= DISK_NUM)
    {
        cout << "无空闲盘块！" << endl;
        return 0;
    }
    // 创建目录初始化
    curDir->directItem[i].sign = 0;
    curDir->directItem[i].firstDisk = j;
    strcpy(curDir->directItem[i].fileName, fName);
    curDir->directItem[i].next = j;
    curDir->directItem[i].type = 1;
    curDir->directItem[i].size = USER_ROOT_SIZE;
    direct *cur_mkdir = (direct *)(fdisk + curDir->directItem[i].firstDisk * DISK_SIZE); // 创建目录的物理地址
    // 指向当前目录的目录项
    cur_mkdir->directItem[0].sign = 0;
    cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
    strcpy(cur_mkdir->directItem[0].fileName, ".");
    cur_mkdir->directItem[0].next = cur_mkdir->directItem[0].firstDisk;
    cur_mkdir->directItem[0].type = 1;
    cur_mkdir->directItem[0].size = USER_ROOT_SIZE;
    // 指向上一级目录的目录项
    cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
    cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
    strcpy(cur_mkdir->directItem[1].fileName, "..");
    cur_mkdir->directItem[1].next = cur_mkdir->directItem[1].firstDisk;
    cur_mkdir->directItem[1].type = 1;
    cur_mkdir->directItem[1].size = USER_ROOT_SIZE;
    // 子目录初始化
    for (int i = 2; i < MSD + 2; i++)
    {
        cur_mkdir->directItem[i].sign = 0;
        cur_mkdir->directItem[i].firstDisk = -1;
        strcpy(cur_mkdir->directItem[i].fileName, "");
        cur_mkdir->directItem[i].next = -1;
        cur_mkdir->directItem[i].type = 0;
        cur_mkdir->directItem[i].size = 0;
    }
    return 1;
}

/*删除文件*/
int rm(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 在该目录下找目标文件
    int i, temp;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该文件！" << endl;
        return 0;
    }
    int item = curDir->directItem[i].firstDisk; // 目标文件的起始磁盘块号
                                                // 根据文件分配表依次删除文件所占据的磁盘块
    while (item != -1)
    {
        temp = fat[item].item;
        fat[item].item = -1;
        fat[item].state = 0;
        item = temp;
    }
    // 释放目录项
    curDir->directItem[i].sign = 0;
    curDir->directItem[i].firstDisk = -1;
    strcpy(curDir->directItem[i].fileName, "");
    curDir->directItem[i].next = -1;
    curDir->directItem[i].type = 0;
    curDir->directItem[i].size = 0;
    return 1;
}

/*删除目录*/
int rmdir(string *str, int level)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 在当前目录下查询目标目录
    int i, j;
    direct *tempDir = curDir;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(tempDir->directItem[i].fileName, fName) && tempDir->directItem[i].type == 1)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该目录！" << endl;
        return 0;
    }
    // 检查是否有子目录或文件
    tempDir = (direct *)(fdisk + tempDir->directItem[i].firstDisk * DISK_SIZE);
    bool flag = false;
    direct *tempDir2 = curDir;
    for (j = 2; j < MSD + 2; j++)
    {
        if (tempDir->directItem[j].firstDisk != -1)
        {
            if (!flag && !level)
            {
                cout << "该目录下含有子文件或者子目录，是否确认删除(y/n)？" << endl;
                int again = 1;
                while (again)
                {
                    string choice;
                    cin >> choice;
                    strip(choice, ' ');
                    string k;
                    getline(cin, k); // 占位
                    if (choice == "y" || choice == "Y")
                        again = 0;
                    else if (choice == "n" || choice == "N")
                        return 0;
                    else
                        cout << "您的输入有误！请重新输入（y/n）:";
                }
                flag = true;
            }
            curDir = (struct direct *)(fdisk + DISK_SIZE * tempDir->directItem[0].firstDisk);
            string str[2];
            str[1] = tempDir->directItem[j].fileName;
            f if (tempDir->directItem[j].type == 1)
                rmdir(str, ++level);
            else if (tempDir->directItem[j].type == 0)
                rm(str);
        }
    }
    curDir = tempDir2;
    fat[curDir->directItem[i].firstDisk].state = 0; // 修改文件分配表
    // 修改目录项
    curDir->directItem[i].sign = 0;
    curDir->directItem[i].firstDisk = -1;
    strcpy(curDir->directItem[i].fileName, "");
    curDir->directItem[i].next = -1;
    curDir->directItem[i].type = 0;
    curDir->directItem[i].size = 0;
    return 1;
}

// 位示图
void show(string *str)
{
    int i = ParseInt(str[1]), j = ParseInt(str[2]);
    for (int k = i; k <= j; k++)
    {
        cout << fat[k].state << ' ';
        if (k % 5 == 0 && k != 0)
            cout << endl;
    }
}

/*切换目录*/
void cd(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    string objPath = str[1]; // 目标路径
    if (objPath[objPath.size() - 1] != '\\')
        objPath += "\\";
    int start = -1;
    int end = 0; // 以\为分割，每段起始下标和结束下标
    int i, j, len = objPath.length();
    direct *tempDir = curDir;
    string oldPath = curPath; // 保存切换前的路径
    string temp;
    for (i = 0; i < len; i++)
    {
        // 如果目标路径以\开头，则从根目录开始查询
        if (objPath[0] == '\\')
        {
            tempDir = root;
            curPath = userName + ":\\";
            continue;
        }
        if (start == -1)
            start = i;
        if (objPath[i] == '\\')
        {
            end = i;
            temp = objPath.substr(start, end - start);
            // 检查目录是否存在
            for (j = 0; j < MSD + 2; j++)
                if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
                    break;
            if (j >= MSD + 2)
            {
                curPath = oldPath;
                cout << "找不到该目录！" << endl;
                return;
            }
            // 如果目标路径为".."，则返回上一级
            if (temp == "..")
            {
                if (tempDir->directItem[j - 1].sign != 1)
                {
                    int pos = curPath.rfind('\\', curPath.length() - 2);
                    curPath = curPath.substr(0, pos + 1);
                }
            }
            // 如果目标路径为"."，则指向当前目录
            else
            {
                if (temp != ".")
                    curPath = curPath + objPath.substr(start, end - start) + "\\";
            }
            start = -1;
            tempDir = (direct *)(fdisk + tempDir->directItem[j].firstDisk * DISK_SIZE);
        }
    }
    curDir = tempDir;
}

// 打开文件
bool open(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 在当前目录下查找目标文件
    int i, j;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该文件！" << endl;
        return false;
    }
    curDir->directItem[i].isOpened = true;
    return true;
}

// 关闭文件
bool close(string *str)
{
    char fName[20];
    strcpy(fName, str[1].c_str());
    // 在当前目录下查找目标文件
    int i, j;
    for (i = 2; i < MSD + 2; i++)
        if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
            break;
    if (i >= MSD + 2)
    {
        cout << "找不到该文件！" << endl;
        return false;
    }
    curDir->directItem[i].isOpened = false;
    return true;
}

// 格式化磁盘
bool format()
{
    delete[] fdisk;
    init(userDat);
    save();
    readUserDisk(userDat);
    return true;
}

// 帮助列表
void help()
{
    cout << fixed << left;
    cout << "********************  帮助  ********************" << endl
         << endl;
    cout << setw(40) << "cd 路径" << setw(10) << "切换目录" << endl;
    cout << setw(40) << "touch 文件名" << setw(10) << "创建文件" << endl;
    cout << setw(40) << "rm 文件名" << setw(10) << "删除文件" << endl;
    cout << setw(40) << "write 文件名" << setw(10) << "写入文件内容" << endl;
    cout << setw(40) << "read 文件名" << setw(10) << "读取文件内容" << endl;
    cout << setw(40) << "open 文件名" << setw(10) << "打开文件" << endl;
    cout << setw(40) << "close 文件名" << setw(10) << "关闭文件" << endl;
    cout << setw(40) << "mkdir 目录名" << setw(10) << "创建目录" << endl;
    cout << setw(40) << "rmdir 目录名" << setw(10) << "删除目录" << endl;
    cout << setw(40) << "ls" << setw(10) << "显示当前目录下所有子目录和子文件" << endl;
    cout << setw(40) << "show 磁盘起始号 磁盘结束号" << setw(10) << "显示磁盘占用的fat表（位示图）" << endl;
    cout << setw(40) << "save" << setw(10) << "保存磁盘数据" << endl;
    cout << setw(40) << "help" << setw(10) << "帮助文档" << endl;
    cout << setw(40) << "clear" << setw(10) << "清屏" << endl;
    cout << setw(40) << "format" << setw(10) << "格式化" << endl;
    cout << setw(40) << "exit" << setw(10) << "退出" << endl
         << endl;
}

// 菜单栏
void menu()
{
    system("cls");
    getchar();
    cout << "**************** Hello  " << userName << "! ****************" << endl;
    // 如果没有找到磁盘数据文件，则选择是否开始初始化
    fstream fp;
    userDat = userName + ".dat"; // 用户数据
    fp.open(userDat, ios::in | ios::binary);
    if (!fp)
    {
        cout << "您还没有初始化，请问您现在要初始化吗(y/n)" << endl;
        int again = 1;
        while (again)
        {
            string choice;
            cin >> choice;
            strip(choice, ' ');
            getchar();
            if (choice == "y" || choice == "Y")
            {
                again = 0;
                init(userDat);
            }
            else if (choice == "n" || choice == "N")
            {
                again = 0;
                return;
            }
            else
                cout << "您的输入有误！请重新输入（y/n）:";
        }
    }
    fp.close(); // 文件关闭
    readUserDisk(userDat);
    while (true)
    {
        Shell();
        // 以空格为界线，读取命令,并用stringstream流分割命令
        string cmd;
        getline(cin, cmd);
        stringstream stream;
        stream.str(cmd);
        string str[5];
        for (int i = 0; stream >> str[i]; i++)
            ;
        int num = 0;
        for (int i = 0; i < 5; i++)
            if (str[i].length())
                num++;
        Check check[16] = {{2, "cd"}, {2, "touch"}, {2, "rm"}, {2, "write"}, {2, "read"}, {2, "open"}, {2, "close"}, {2, "mkdir"}, {2, "rmdir"}, {1, "ls"}, {3, "show"}, {1, "save"}, {1, "help"}, {1, "clear"}, {1, "format"}, {1, "exit"}};
        int j;
        for (j = 0; j < 16; j++)
            if (str[0] == check[j].s)
                break;
        if (j >= 16)
        {
            cout << "Q&X: command not found!" << endl;
            continue;
        }
        else if (num != check[j].level)
        {
            cout << "Q&X: command incorrect!" << endl;
            continue;
        }
        if (str[0] == "touch")
        { // 创建一个文件
            if (touch(str))
                cout << str[1] << "创建成功！" << endl;
        }
        else if (str[0] == "rm")
        {
            if (rm(str))
            {
                cout << str[1] << "删除成功！" << endl;
            }
        }
        else if (str[0] == "ls")
        { // 显示目录下内容
            ls();
        }
        else if (str[0] == "exit")
        { // 退出命令行
            exit(0);
        }
        else if (str[0] == "mkdir")
        { // 创建一个目录
            if (mkdir(str))
                cout << str[1] << "创建成功！" << endl;
        }
        else if (str[0] == "rmdir")
        { // 删除一个目录
            if (rmdir(str, 0))
                cout << str[1] << "删除成功！" << endl;
        }
        else if (str[0] == "show")
        { // 显示 FAT表位示图
            show(str);
        }
        else if (str[0] == "read")
        {
            char *buf = new char[MAX_FILE_SIZE]; // 设置缓冲区
            if (read(str, buf))
            {
                cout << "*************************************" << endl;
                if (strlen(buf))
                    cout << buf << endl;
                cout << "*************************************" << endl;
                delete[] buf;
            }
        }

        else if (str[0] == "save")
        {
            if (save())
                cout << "保存成功！" << endl;
            else
                cout << "保存失败" << endl;
        }
        else if (str[0] == "write")
        {
            // 检查当前目录下是否有目标文件
            int i;
            for (i = 2; i < MSD + 2; i++)
                if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
                    break;
            if (i >= MSD + 2)
            {
                cout << "找不到该文件！" << endl;
                continue;
            }
            if (curDir->directItem[i].isOpened == false)
            {
                cout << "文件未打开，请先打开文件" << endl;
                continue;
            }

            // 输入写入内容，并以#号结束
            char ch;
            char content[MAX_FILE_SIZE]; // 设置缓冲区
            int size = 0;
            cout << "请输入文件内容，并以'#'为结束标志" << endl;
            while (1)
            {
                ch = getchar();
                if (ch == '#')
                    break;
                if (size >= MAX_FILE_SIZE)
                {
                    cout << "输入文件内容过长！" << endl;
                    break;
                }
                content[size] = ch;
                size++;
            }
            if (size >= MAX_FILE_SIZE)
                continue;
            getchar();
            write(str, content, size);
        }
        else if (str[0] == "cd")
        {
            cd(str);
        }
        else if (str[0] == "clear")
        {
            system("cls");
            system("color 0B");
            cout << "**************** Hello  " << userName << "! ****************" << endl;
        }
        else if (str[0] == "open")
        {
            if (open(str))
                cout << "文件" << str[1] << "打开成功！" << endl;
        }
        else if (str[0] == "close")
        {
            if (close(str))
                cout << "文件" << str[1] << "关闭成功！" << endl;
        }
        else if (str[0] == "format")
        {
            if (format())
                cout << "格式化成功！" << endl;
        }
        else if (str[0] == "help")
        {
            help();
        }
    }
}

// 用户登录功能
void Login()
{
    // 创建用户目录表
    fstream fp;
    fp.open(userLogin, ios::in);
    if (!fp)
    {
        fp.close();
        fp.open(userLogin, ios::out);
        fp.close();
    }
    int successFlag = 0; // 标记是否终止登录
    while (!successFlag)
    {
        int haveUser = 0; // 标记是否存在该用户
        // 验证用户名
        string inputName;
        cout << "用户名:";
        cin >> inputName;
        strip(inputName, ' '); // 输入并处理用户名
        ifstream fp;
        fp.open(userLogin);
        if (fp)
        {
            string userLine;
            while (getline(fp, userLine))
            {
                string name, password;
                halfStr(name, password, userLine);
                if (name == inputName)
                {
                    haveUser = 1; // 标记确实有该用户
                    userName = name;
                    userPassword = password;
                }
            }
            fp.close();
        }
        else
        {
            cout << userLogin << "打开错误！" << endl;
            return;
        }
        // 如果找到了用户名，则输入密码
        if (haveUser)
        {
            int flag = 1;  // 标记是否需要重新出入密码
            int times = 0; // 输入密码的次数，如果输入错误大于3次，则终止登录
            while (flag)
            {
                string inputPassword;
                cout << "密码:";
                cin >> inputPassword;
                strip(inputPassword, ' ');
                // 如果密码输入正确！
                if (inputPassword == userPassword)
                {
                    flag = 0;
                    menu();          // 进入功能用户界面
                    successFlag = 1; // 成功登录
                }
                // 密码输入错误
                else
                {
                    times++;
                    if (times < 3)
                    { // 选择是否重新输入密码
                        cout << "输入密码错误！" << endl
                             << "是否重新输入（y/n）?:";
                        int again = 1;
                        while (again)
                        {
                            string choice;
                            cin >> choice;
                            strip(choice, ' ');
                            if (choice == "y" || choice == "Y")
                                again = 0;
                            else if (choice == "n" || choice == "N")
                            {
                                flag = 0;
                                again = 0;
                                return;
                            }
                            else
                                cout << "您的输入有误！请重新输入（y/n）:";
                        }
                    }
                    else
                    { // 密码输入错误达到三次则直接退出
                        cout << "输入密码错误已达到3次！" << endl;
                        flag = 0;
                        return;
                    }
                }
            }
        }
        else
        { // 如果没有找到用户名，报错并请用户选择是否重新输入
            int again = 1;
            cout << "抱歉没有找到该用户！" << endl
                 << "是否重新输入（y/n）?" << endl;
            while (again)
            {
                string choice;
                cin >> choice;
                strip(choice, ' ');
                if (choice == "y" || choice == "Y")
                    again = 0;
                else if (choice == "n" || choice == "N")
                {
                    again = 0;
                    return;
                }
                else
                    cout << "您的输入有误！请重新输入（y/n）:";
            }
        }
    }
}

// 用户注册功能
void Register()
{
    // 创建用户目录表
    fstream fp;
    fp.open(userLogin, ios::in);
    // 如果不存在目录表，默认创建一个
    if (!fp)
    {
        fp.close();
        fp.open(userLogin, ios::out);
        fp.close();
    }
    int flag = 1; // 标记是否终止注册
    while (flag)
    {
        string inputName, password, password2; // 用户名，密码，确认密码
        int used = 0;                          // 标记用户名是否已被使用过
        cout << "请输入注册用户名：";
        cin >> inputName;
        // 写入用户名
        ifstream fp;
        fp.open(userLogin);
        if (fp)
        {
            // 从文件中按行读取用户信息，并根据空格划分用户名和密码
            string userLine;
            while (getline(fp, userLine))
            {
                string name, password;
                halfStr(name, password, userLine);
                if (name == inputName)
                {
                    used = 1; // 标记已存在该用户
                }
            }
            fp.close();
        }
        else
        {
            cout << userLogin << "打开错误！" << endl;
            return;
        }

        // 验证密码
        if (!used)
        {
            cout << "请输入密码：";
            cin >> password;
            cout << "请再次输入密码：";
            cin >> password2;
            // 如果两次输入密码都有效且一致，则保存用户名和密码，并提示注册成功
            if (password == password2)
            {
                fstream fp;
                fp.open(userLogin, ios::app); // 打开文件并且输入内容到文件最后一行
                if (fp)
                {
                    fp << inputName << " " << password2 << endl;
                    fp.close();
                    cout << "注册成功！" << endl;
                }
                else
                {
                    cout << userLogin << "打开错误！" << endl;
                    return;
                }
                flag = 0;
            }
            // 如果两次输入密码不一致，则提示是否重新输入
            else
            {
                int again = 1;
                cout << "两次密码输入不一致！" << endl
                     << "是否重新输入（y/n）?:";
                while (again)
                {
                    string choice;
                    cin >> choice;
                    strip(choice, ' ');
                    if (choice == "y" || choice == "Y")
                    {
                        again = 0;
                    }
                    else if (choice == "n" || choice == "N")
                    {
                        again = 0;
                        return;
                    }
                    else
                    {
                        cout << "您的输入有误！请重新输入（y/n）:";
                    }
                }
            }
        }
        // 如果用户名已被使用，则提示是否重新输入
        else
        {
            int again = 1;
            cout << "用户名已存在！" << endl
                 << "是否重新输入（y/n）?" << endl;
            while (again)
            {
                string choice;
                cin >> choice;
                strip(choice, ' ');
                if (choice == "y" || choice == "Y")
                {
                    again = 0;
                }
                else if (choice == "n" || choice == "N")
                {
                    again = 0;
                    return;
                }
                else
                {
                    cout << "您的输入有误！请重新输入（y/n）:";
                }
            }
        }
    }
}

void searchUser()
{
    ifstream fp;
    fp.open(userLogin);
    if (fp)
    {
        string userLine;
        while (getline(fp, userLine))
        {
            string name, password;
            halfStr(name, password, userLine);
            cout << name << endl;
        }
    }
    fp.close();
}
// ）主函数（开始界面
int main()
{
    system("color 0E");
    while (true)
    {
        cout << "****************欢迎进入Q&X文件系统****************" << endl;
        cout << "----------------------------------------------------" << endl;
        cout << "****************************************************" << endl
             << endl;
        cout << "  1、登录   2、注册     3.用户列表      0、退出	" << endl
             << endl;
        cout << "****************************************************" << endl;
        cout << "请输入：";
        int choice;
        cin >> choice;
        switch (choice)
        {
        case 1:
            Login();
            break;
        case 2:
            Register();
            getchar();
            break;
        case 3:
            searchUser();
            getchar();
            break;
        case 0:
            return 0;
        default:
            cout << "您的输入有误！请重新输入！" << endl;
            break;
        }
        cout << "\n\n\n请按回车键继续......" << endl;
        getchar();
        system("cls");
    }
}
