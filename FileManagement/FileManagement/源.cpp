#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<algorithm>
#include<conio.h>
#include<fstream>
#include<string>
#include<sstream>
#include<iomanip>
#include<windows.h>
using namespace std;





#define MEM_SIZE 1024*1024 //�ܴ��̿ռ�Ϊ1M 1024*1024bytes 1byte=8bit
#define DISK_SIZE 1024 //���̿��С1K
#define DISK_NUM 1024//һ����1k�����̿�
#define MAX_FILE_SIZE 10240//����ļ�����
#define MSD 7//�����Ŀ¼��max subdirect
#define FATSIZE DISK_NUM*sizeof(fatItem)//FAT���С���洢�ռ��С��
#define USER_ROOT_STARTBLOCK FATSIZE/DISK_SIZE+1//�û���Ŀ¼��ʼ�̿��  һ��Fat��ռ�ü������̿�
#define USER_ROOT_SIZE sizeof(direct)//�û���Ŀ¼��С 396




//FCB�ļ����ƿ�
typedef struct FCB {//�ļ����ƿ� һ����44��bytes
	char fileName[20];//�ļ���Ŀ¼��
	int type;// 0--�ļ�  1--Ŀ¼ 
	char* content = NULL;//�ļ�����
	int size;//��С
	int firstDisk;//��ʼ�̿�
	int next;//��Ŀ¼��ʼ�̿��
	int sign;//0--��ͨĿ¼  1--��Ŀ¼
	bool isOpened = false;
}FCB;

//Ŀ¼
struct direct {
	FCB directItem[MSD + 2];
};

//���shell����
struct Check {
	int level;
	string s;
};


//�ļ������
typedef struct fatItem {
	int item;//����ļ���һ�����̵�ָ��
	int state;
}fatItem, * FAT;
// fatItem fatItem
//fatItem *FAT




string userLogin = "userfile.txt";//�û�Ŀ¼�����ڼ�¼�û���������
string userName, userPassword;//��ǰ�û���������
string userDat;//�û���������
FAT fat;//FAT��
direct* root; //��Ŀ¼
direct* curDir; //��ǰĿ¼
string curPath; //��ǰ·��
char* fdisk;//���������ʼ��ַ










//�ַ���stringת��Ϊint����
int ParseInt(string s)
{
	int l = s.length();
	int a[7];
	for (int i = 0; i < l; i++) a[i + 1] = s[i] - '0';
	int num = 0;
	for (int i = 1; i <= l; i++)
	{
		num *= 10;
		num += a[i];
	}
	return num;
}

//ȥ���߿ո�
void strip(string& str, char ch)
{
	int i = 0;
	while (str[i] == ch)i++;
	int j = str.size() - 1;
	while (str[j] == ch)j--;
	str = str.substr(i, j + 1 - i);
}

//�����û���������
void halfStr(string& userName, string& password, string line)
{
	int i;
	int len = line.length();
	for (i = 0; i < len; i++)
		if (line[i] == ' ')break;
	userName = line.substr(0, i);
	password = line.substr(i + 1, len);
}

void Shell()
{
	//�����û�����·����ǰ��ɫ�ͱ���ɫ
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
	cout << curPath << ">";//����û�����·��
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	//���ԭ������ɫ
}

/*�����������*/
int save()
{
	ofstream fp;
	fp.open(userDat, ios::out | ios::binary);
	if (fp) {
		fp.write(fdisk, MEM_SIZE * sizeof(char));//��fdiskд��userDat�ļ�
		fp.close();
		return 1;
	}
	else {
		cout << userDat << "��ʧ�ܣ�" << endl;
		fp.close();
		return 0;
	}
}

//��ʼ�����̽ṹ
void init(string userDat)
{
	fdisk = new char[MEM_SIZE * sizeof(char)];//������̿ռ� 1024*1024*char
	for (int i = 0; i < MEM_SIZE * sizeof(char); i++)
	{
		fdisk[i] = '+';
	}
	//��ʼ��FAT��
	fat = (FAT)(fdisk + DISK_SIZE);//fat��ַ ָ���һ�����̿�ĵ�ַ
	fat[0].item = -1; //ָ����
	fat[0].state = 1;//��ʼ��FAT��
	for (int i = 1; i < USER_ROOT_STARTBLOCK; i++) { // <= ???
		fat[i].item = i + 1;
		fat[i].state = 1;
	}// ��1-8������������� FAT��
	fat[USER_ROOT_STARTBLOCK].item = -1; fat[USER_ROOT_STARTBLOCK].state = 1;//��Ŀ¼���̿��
	//fat�������ʼ��
	for (int i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++) {
		fat[i].item = -1;
		fat[i].state = 0;
	}
	//��Ŀ¼��ʼ��
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE);//��Ŀ¼��ַ
	root->directItem[0].sign = 1;
	root->directItem[0].firstDisk = USER_ROOT_STARTBLOCK; // 9
	strcpy(root->directItem[0].fileName, ".");// ��Ŀ¼����Ϊ .
	root->directItem[0].next = root->directItem[0].firstDisk;
	root->directItem[0].type = 1;
	root->directItem[0].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
	root->directItem[1].sign = 1;
	root->directItem[1].firstDisk = USER_ROOT_STARTBLOCK;
	strcpy(root->directItem[1].fileName, "..");
	root->directItem[1].next = root->directItem[0].firstDisk;
	root->directItem[1].type = 1;
	root->directItem[1].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
	for (int i = 2; i < MSD + 2; i++) {
		root->directItem[i].sign = 0;
		root->directItem[i].firstDisk = -1;
		strcpy(root->directItem[i].fileName, "");
		root->directItem[i].next = -1;
		root->directItem[i].type = 0;
		root->directItem[i].size = 0;
	}
	if (!save())cout << "��ʼ��ʧ�ܣ�" << endl;
}

/*��ȡ�û�������������*/
void readUserDisk(string userDat)
{
	fdisk = new char[MEM_SIZE];//������̴�С������
	ifstream fp;
	fp.open(userDat, ios::in | ios::binary);
	if (fp)
		fp.read(fdisk, MEM_SIZE);
	else
		cout << userDat << "��ʧ�ܣ�" << endl;
	fp.close();
	fat = (FAT)(fdisk + DISK_SIZE);//fat��ַ
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE);//��Ŀ¼��ַ
	curDir = root;
	curPath = userName + ":\\";
}

/*д�ļ�����*/
void write(string* str, char* content, int size)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return;
	}

	int cur = i;//��ǰĿ¼����±�
	int fSize = curDir->directItem[cur].size;//Ŀ���ļ���С
	int item = curDir->directItem[cur].firstDisk;//Ŀ���ļ�����ʼ���̿��
	while (fat[item].item != -1)item = fat[item].item;//���㱣����ļ������һ���̿��
	char* first = fdisk + item * DISK_SIZE + fSize % DISK_SIZE;//������ļ���ĩ��ַ
	//����̿�ʣ�ಿ�ֹ�д����ֱ��д��ʣ�ಿ��
	if (DISK_SIZE - fSize % DISK_SIZE > size) {
		strcpy(first, content);
		curDir->directItem[cur].size += size;
	}
	//����̿�ʣ�ಿ�ֲ���д�����ҵ����д��̿�д��
	else {
		//�Ƚ���ʼ����ʣ�ಿ��д��
		for (j = 0; j < DISK_SIZE - fSize % DISK_SIZE; j++) {
			first[j] = content[j];
		}
		int res_size = size - (DISK_SIZE - fSize % DISK_SIZE);//ʣ��Ҫд�����ݴ�С
		int needDisk = res_size / DISK_SIZE;//ռ�ݵĴ��̿�����
		int needRes = res_size % DISK_SIZE;//ռ�����һ����̿�Ĵ�С
		if (needDisk > 0)needRes += 1;
		for (j = 0; j < needDisk; j++) {
			for (i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
				if (fat[i].state == 0) break;
			if (i >= DISK_NUM) {
				cout << "�����ѱ������꣡" << endl;
				return;
			}
			first = fdisk + i * DISK_SIZE;//���д�����ʼ�������ַ
			//��д�����һ����̣���ֻдʣ�ಿ������
			if (j == needDisk - 1) {
				for (int k = 0; k < size - (DISK_SIZE - fSize % DISK_SIZE - j * DISK_SIZE); k++)
					first[k] = content[k];
			}
			else {
				for (int k = 0; k < DISK_SIZE; k++)
					first[k] = content[k];
			}
			//�޸��ļ����������
			fat[item].item = i;
			fat[i].state = 1;
			fat[i].item = -1;
		}
		curDir->directItem[cur].size += size;
	}
}

//��ʾ�ļ�����
bool read(string* str, char* buf)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//����Ŀ¼���Ƿ���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return 0;
	}
	if (curDir->directItem[i].isOpened == false) {
		cout << "�ļ�δ�򿪣����ȴ��ļ�" << endl;
		return 0;
	}
	int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
	int fSize = curDir->directItem[i].size;//Ŀ���ļ��Ĵ�С
	int needDisk = fSize / DISK_SIZE;//ռ�ݵĴ��̿�����
	int needRes = fSize % DISK_SIZE;//ռ�����һ����̿�Ĵ�С
	if (needRes > 0)needDisk += 1;
	char* first = fdisk + item * DISK_SIZE;//��ʼ���̿�������ַ
	//��Ŀ¼�ļ����ݿ�������������
	if (fSize <= 0) {
		buf[0] = '\0';
		return 0;
	}
	for (i = 0; i < needDisk; i++) {
		for (j = 0; j < fSize - i * DISK_SIZE; j++) {
			buf[i * DISK_SIZE + j] = first[j];
		}
		if (i == needDisk - 1 && j == fSize - i * DISK_SIZE)
			buf[i * DISK_SIZE + j] = '\0';
		if (i != needDisk - 1) {
			item = fat[item].item;
			first = fdisk + item * DISK_SIZE;
		}
	}
	return 1;
}

/*touch�����ļ�*/
int touch(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());//c_strת��Ϊ�ַ�������
	//����ļ����Ƿ�Ϸ�
	if (!strcmp(fName, ""))
	{
		puts("����ʧ��!�������ļ�����");
		return 0;
	}
	if (str[1].length() > 20) {
		cout << "�ļ���̫�������������룡" << endl;
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ����ļ�����
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			cout << "��ǰĿ¼���Ѵ����ļ�����!" << endl;
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "��ǰĿ¼�¿ռ�����" << endl;
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "�޿����̿飡" << endl;
		return 0;
	}
	//�ļ���ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	curDir->directItem[i].isOpened = false;


	return 1;
}

/*��ʾ��ǰĿ¼����Ŀ¼���ļ���Ϣ*/
void ls()
{
	cout << fixed << left << setw(20) << "�ļ���" << setw(10) << "����" << setw(10) << "��С</B>" << setw(10) << "��ʼ���̿��" << endl;
	for (int i = 2; i < MSD + 2; i++) {
		if (curDir->directItem[i].firstDisk != -1) {
			cout << fixed << setw(20) << curDir->directItem[i].fileName;
			if (curDir->directItem[i].type == 0) {//��ʾ�ļ�
				cout << fixed << setw(10) << " " << setw(10) << curDir->directItem[i].size;
			}
			else {//��ʾĿ¼
				cout << fixed << setw(10) << "<DIR>" << setw(10) << " ";
			}
			cout << setw(10) << curDir->directItem[i].firstDisk << endl;
		}
	}
}

/*����Ŀ¼*/
int mkdir(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//���Ŀ¼���Ƿ�Ϸ�
	if (!strcmp(fName, ""))return 0;
	if (str[1].length() > 10) {
		cout << "Ŀ¼��̫�������������룡" << endl;
		return 0;
	}
	if (!strcmp(fName, ".") || !strcmp(fName, "..")) {
		cout << "Ŀ¼���Ʋ��Ϸ���" << endl;
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ���Ŀ¼����
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 1) {
			cout << "��ǰĿ¼���Ѵ���Ŀ¼����!" << endl;
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "��ǰĿ¼�¿ռ�����" << endl;
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "�޿����̿飡" << endl;
		return 0;
	}
	//����Ŀ¼��ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j;
	curDir->directItem[i].type = 1;
	curDir->directItem[i].size = USER_ROOT_SIZE;
	direct* cur_mkdir = (direct*)(fdisk + curDir->directItem[i].firstDisk * DISK_SIZE);//����Ŀ¼�������ַ
	//ָ��ǰĿ¼��Ŀ¼��
	cur_mkdir->directItem[0].sign = 0;
	cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
	strcpy(cur_mkdir->directItem[0].fileName, ".");
	cur_mkdir->directItem[0].next = cur_mkdir->directItem[0].firstDisk;
	cur_mkdir->directItem[0].type = 1;
	cur_mkdir->directItem[0].size = USER_ROOT_SIZE;
	//ָ����һ��Ŀ¼��Ŀ¼��
	cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
	cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
	strcpy(cur_mkdir->directItem[1].fileName, "..");
	cur_mkdir->directItem[1].next = cur_mkdir->directItem[1].firstDisk;
	cur_mkdir->directItem[1].type = 1;
	cur_mkdir->directItem[1].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
	for (int i = 2; i < MSD + 2; i++) {
		cur_mkdir->directItem[i].sign = 0;
		cur_mkdir->directItem[i].firstDisk = -1;
		strcpy(cur_mkdir->directItem[i].fileName, "");
		cur_mkdir->directItem[i].next = -1;
		cur_mkdir->directItem[i].type = 0;
		cur_mkdir->directItem[i].size = 0;
	}
	return 1;
}

/*ɾ���ļ�*/
int rm(string* str) {
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڸ�Ŀ¼����Ŀ���ļ�
	int i, temp;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return 0;
	}
	int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
		//�����ļ����������ɾ���ļ���ռ�ݵĴ��̿�
	while (item != -1)
	{
		temp = fat[item].item;
		fat[item].item = -1;
		fat[item].state = 0;
		item = temp;
	}
	//�ͷ�Ŀ¼��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}

/*ɾ��Ŀ¼*/
int rmdir(string* str, int level)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²�ѯĿ��Ŀ¼
	int i, j; direct* tempDir = curDir;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(tempDir->directItem[i].fileName, fName) && tempDir->directItem[i].type == 1)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ�����Ŀ¼��" << endl;
		return 0;
	}
	//����Ƿ�����Ŀ¼���ļ�
	tempDir = (direct*)(fdisk + tempDir->directItem[i].firstDisk * DISK_SIZE);
	bool flag = false;
	direct* tempDir2 = curDir;
	for (j = 2; j < MSD + 2; j++) {
		if (tempDir->directItem[j].firstDisk != -1) {
			if (!flag && !level) {
				cout << "��Ŀ¼�º������ļ�������Ŀ¼���Ƿ�ȷ��ɾ��(y/n)��" << endl;
				int again = 1;
				while (again)
				{
					string choice; cin >> choice; strip(choice, ' ');
					string k; getline(cin, k);//ռλ
					if (choice == "y" || choice == "Y") again = 0;
					else if (choice == "n" || choice == "N") return 0;
					else
						cout << "���������������������루y/n��:";
				}
				flag = true;
			}
			curDir = (struct direct*)(fdisk + DISK_SIZE * tempDir->directItem[0].firstDisk);
			string str[2];
			str[1] = tempDir->directItem[j].fileName;
			if (tempDir->directItem[j].type == 1)rmdir(str, ++level);
			else if (tempDir->directItem[j].type == 0)rm(str);
		}
	}
	curDir = tempDir2;
	fat[curDir->directItem[i].firstDisk].state = 0;//�޸��ļ������
	//�޸�Ŀ¼��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}

//λʾͼ
void show(string* str) {
	int i = ParseInt(str[1]), j = ParseInt(str[2]);
	for (int k = i; k <= j; k++) {
		cout << fat[k].state << ' ';
		if (k % 5 == 0 && k != 0)cout << endl;
	}
}

/*�л�Ŀ¼*/
void cd(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	string objPath = str[1];//Ŀ��·��
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1; int end = 0;//��\Ϊ�ָÿ����ʼ�±�ͽ����±�
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//�����л�ǰ��·��
	string temp;
	for (i = 0; i < len; i++) {
		//���Ŀ��·����\��ͷ����Ӹ�Ŀ¼��ʼ��ѯ
		if (objPath[0] == '\\') {
			tempDir = root;
			curPath = userName + ":\\";
			continue;
		}
		if (start == -1)
			start = i;
		if (objPath[i] == '\\') {
			end = i;
			temp = objPath.substr(start, end - start);
			//���Ŀ¼�Ƿ����
			for (j = 0; j < MSD + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= MSD + 2) {
				curPath = oldPath;
				cout << "�Ҳ�����Ŀ¼��" << endl;
				return;
			}
			//���Ŀ��·��Ϊ".."���򷵻���һ��
			if (temp == "..") {
				if (tempDir->directItem[j - 1].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//���Ŀ��·��Ϊ"."����ָ��ǰĿ¼
			else {
				if (temp != ".")
					curPath = curPath + objPath.substr(start, end - start) + "\\";
			}
			start = -1;
			tempDir = (direct*)(fdisk + tempDir->directItem[j].firstDisk * DISK_SIZE);
		}
	}
	curDir = tempDir;
}

//���ļ�
bool open(string* str) {
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return false;
	}
	curDir->directItem[i].isOpened = true;
	return true;
}

//�ر��ļ�
bool close(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return false;
	}
	curDir->directItem[i].isOpened = false;
	return true;
}

//��ʽ������
bool format()
{
	delete[]fdisk;
	init(userDat);
	save();
	readUserDisk(userDat);
	return true;
}

//�����б�
void help()
{
	cout << fixed << left;
	cout << "********************  ����  ********************" << endl << endl;
	cout << setw(40) << "cd ·��" << setw(10) << "�л�Ŀ¼" << endl;
	cout << setw(40) << "touch �ļ���" << setw(10) << "�����ļ�" << endl;
	cout << setw(40) << "rm �ļ���" << setw(10) << "ɾ���ļ�" << endl;
	cout << setw(40) << "write �ļ���" << setw(10) << "д���ļ�����" << endl;
	cout << setw(40) << "read �ļ���" << setw(10) << "��ȡ�ļ�����" << endl;
	cout << setw(40) << "open �ļ���" << setw(10) << "���ļ�" << endl;
	cout << setw(40) << "close �ļ���" << setw(10) << "�ر��ļ�" << endl;
	cout << setw(40) << "mkdir Ŀ¼��" << setw(10) << "����Ŀ¼" << endl;
	cout << setw(40) << "rmdir Ŀ¼��" << setw(10) << "ɾ��Ŀ¼" << endl;
	cout << setw(40) << "ls" << setw(10) << "��ʾ��ǰĿ¼��������Ŀ¼�����ļ�" << endl;
	cout << setw(40) << "show ������ʼ�� ���̽�����" << setw(10) << "��ʾ����ռ�õ�fat��λʾͼ��" << endl;
	cout << setw(40) << "save" << setw(10) << "�����������" << endl;
	cout << setw(40) << "help" << setw(10) << "�����ĵ�" << endl;
	cout << setw(40) << "clear" << setw(10) << "����" << endl;
	cout << setw(40) << "format" << setw(10) << "��ʽ��" << endl;
	cout << setw(40) << "exit" << setw(10) << "�˳�" << endl << endl;
}

//�˵���
void menu()
{
	system("cls"); getchar();
	cout << "**************** Hello  " << userName << "! ****************" << endl;
	//���û���ҵ����������ļ�����ѡ���Ƿ�ʼ��ʼ��
	fstream fp;
	userDat = userName + ".dat";//�û�����
	fp.open(userDat, ios::in | ios::binary);
	if (!fp) {
		cout << "����û�г�ʼ��������������Ҫ��ʼ����(y/n)" << endl;
		int again = 1;
		while (again)
		{
			string choice; cin >> choice; strip(choice, ' '); getchar();
			if (choice == "y" || choice == "Y") {
				again = 0;
				init(userDat);
			}
			else if (choice == "n" || choice == "N") {
				again = 0;
				return;
			}
			else
				cout << "���������������������루y/n��:";
		}
	}
	fp.close(); //�ļ��ر�
	readUserDisk(userDat);
	while (true)
	{
		Shell();
		//�Կո�Ϊ���ߣ���ȡ����,����stringstream���ָ�����
		string cmd; getline(cin, cmd);
		stringstream stream; stream.str(cmd);
		string str[5];
		for (int i = 0; stream >> str[i]; i++);
		int num = 0;
		for (int i = 0; i < 5; i++)if (str[i].length())num++;
		Check check[16] = { {2,"cd"},{2,"touch"},{2,"rm"},{2,"write"},{2,"read"},{2,"open"},{2,"close"},{2,"mkdir"},{2,"rmdir"},{1,"ls"},{3,"show"},{1,"save"},{1,"help"},{1,"clear"},{1,"format"},{1,"exit"} };
		int j;
		for (j = 0; j < 16; j++)if (str[0] == check[j].s)break;
		if (j >= 16)
		{
			cout << "Q&X: command not found!" << endl;
			continue;
		}
		else
			if (num != check[j].level)
			{
				cout << "Q&X: command incorrect!" << endl;
				continue;
			}
		if (str[0] == "touch") {//����һ���ļ�
			if (touch(str))
				cout << str[1] << "�����ɹ���" << endl;
		}
		else if (str[0] == "rm") {
			if (rm(str)) {
				cout << str[1] << "ɾ���ɹ���" << endl;
			}
		}
		else if (str[0] == "ls") {//��ʾĿ¼������
			ls();
		}
		else if (str[0] == "exit") {//�˳�������
			exit(0);
		}
		else if (str[0] == "mkdir") {//����һ��Ŀ¼
			if (mkdir(str))
				cout << str[1] << "�����ɹ���" << endl;
		}
		else if (str[0] == "rmdir") {//ɾ��һ��Ŀ¼
			if (rmdir(str, 0))
				cout << str[1] << "ɾ���ɹ���" << endl;
		}
		else if (str[0] == "show") {//��ʾ FAT��λʾͼ
			show(str);
		}
		else if (str[0] == "read") {
			char* buf = new char[MAX_FILE_SIZE];//���û�����
			if (read(str, buf))
			{
				cout << "*************************************" << endl;
				if (strlen(buf)) cout << buf << endl;
				cout << "*************************************" << endl;
				delete[] buf;
			}
		}

		else if (str[0] == "save") {
			if (save())
				cout << "����ɹ���" << endl;
			else
				cout << "����ʧ��" << endl;
		}
		else if (str[0] == "write") {
			//��鵱ǰĿ¼���Ƿ���Ŀ���ļ�
			int i;
			for (i = 2; i < MSD + 2; i++)
				if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
					break;
			if (i >= MSD + 2) {
				cout << "�Ҳ������ļ���" << endl;
				continue;
			}
			if (curDir->directItem[i].isOpened == false) {
				cout << "�ļ�δ�򿪣����ȴ��ļ�" << endl;
				continue;
			}

			//����д�����ݣ�����#�Ž���
			char ch; char content[MAX_FILE_SIZE];//���û�����
			int size = 0;
			cout << "�������ļ����ݣ�����'#'Ϊ������־" << endl;
			while (1)
			{
				ch = getchar();
				if (ch == '#')break;
				if (size >= MAX_FILE_SIZE) {
					cout << "�����ļ����ݹ�����" << endl;
					break;
				}
				content[size] = ch;
				size++;
			}
			if (size >= MAX_FILE_SIZE) continue;
			getchar();
			write(str, content, size);
		}
		else if (str[0] == "cd") {
			cd(str);
		}
		else if (str[0] == "clear") {
			system("cls");
			system("color 0B");
			cout << "**************** Hello  " << userName << "! ****************" << endl;
		}
		else if (str[0] == "open") {
			if (open(str))cout << "�ļ�" << str[1] << "�򿪳ɹ���" << endl;
		}
		else if (str[0] == "close") {
			if (close(str))cout << "�ļ�" << str[1] << "�رճɹ���" << endl;
		}
		else if (str[0] == "format") {
			if (format()) cout << "��ʽ���ɹ���" << endl;
		}
		else if (str[0] == "help") {
			help();
		}
	}

}

//�û���¼����
void Login() {
	//�����û�Ŀ¼��
	fstream fp;
	fp.open(userLogin, ios::in);
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int successFlag = 0;//����Ƿ���ֹ��¼
	while (!successFlag)
	{
		int haveUser = 0;//����Ƿ���ڸ��û�
		//��֤�û���
		string inputName;
		cout << "�û���:"; cin >> inputName; strip(inputName, ' ');//���벢�����û���
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					haveUser = 1;//���ȷʵ�и��û�
					userName = name;
					userPassword = password;
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "�򿪴���" << endl;
			return;
		}
		//����ҵ����û���������������
		if (haveUser) {
			int flag = 1;//����Ƿ���Ҫ���³�������
			int times = 0;//��������Ĵ������������������3�Σ�����ֹ��¼
			while (flag)
			{
				string inputPassword;
				cout << "����:"; cin >> inputPassword; strip(inputPassword, ' ');
				//�������������ȷ��
				if (inputPassword == userPassword) {
					flag = 0;
					menu();//���빦���û�����
					successFlag = 1;//�ɹ���¼
				}
				//�����������
				else {
					times++;
					if (times < 3) {//ѡ���Ƿ�������������
						cout << "�����������" << endl << "�Ƿ��������루y/n��?:";
						int again = 1;
						while (again)
						{
							string choice; cin >> choice; strip(choice, ' ');
							if (choice == "y" || choice == "Y")
								again = 0;
							else if (choice == "n" || choice == "N") {
								flag = 0;
								again = 0;
								return;
							}
							else
								cout << "���������������������루y/n��:";
						}
					}
					else {//�����������ﵽ������ֱ���˳�
						cout << "������������Ѵﵽ3�Σ�" << endl;
						flag = 0;
						return;
					}

				}
			}
		}
		else {//���û���ҵ��û������������û�ѡ���Ƿ���������		
			int again = 1;
			cout << "��Ǹû���ҵ����û���" << endl << "�Ƿ��������루y/n��?" << endl;
			while (again)
			{
				string choice; cin >> choice; strip(choice, ' ');
				if (choice == "y" || choice == "Y")
					again = 0;
				else if (choice == "n" || choice == "N") {
					again = 0;
					return;
				}
				else
					cout << "���������������������루y/n��:";
			}
		}
	}
}

//�û�ע�Ṧ��
void Register() {
	//�����û�Ŀ¼��
	fstream fp;
	fp.open(userLogin, ios::in);
	//���������Ŀ¼��Ĭ�ϴ���һ��
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int flag = 1;//����Ƿ���ֹע��
	while (flag)
	{
		string inputName, password, password2;//�û��������룬ȷ������
		int used = 0;//����û����Ƿ��ѱ�ʹ�ù�
		cout << "������ע���û�����";
		cin >> inputName;
		//д���û���
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			//���ļ��а��ж�ȡ�û���Ϣ�������ݿո񻮷��û���������
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					used = 1;//����Ѵ��ڸ��û�
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "�򿪴���" << endl;
			return;
		}

		//��֤����
		if (!used) {
			cout << "���������룺"; cin >> password;
			cout << "���ٴ��������룺"; cin >> password2;
			//��������������붼��Ч��һ�£��򱣴��û��������룬����ʾע��ɹ�
			if (password == password2) {
				fstream fp;
				fp.open(userLogin, ios::app);//���ļ������������ݵ��ļ����һ��
				if (fp) {
					fp << inputName << " " << password2 << endl;
					fp.close();
					cout << "ע��ɹ���" << endl;
				}
				else {
					cout << userLogin << "�򿪴���" << endl;
					return;
				}
				flag = 0;
			}
			//��������������벻һ�£�����ʾ�Ƿ���������
			else {
				int again = 1;
				cout << "�����������벻һ�£�" << endl << "�Ƿ��������루y/n��?:";
				while (again)
				{
					string choice; cin >> choice; strip(choice, ' ');
					if (choice == "y" || choice == "Y") {
						again = 0;
					}
					else if (choice == "n" || choice == "N")
					{
						again = 0;
						return;
					}
					else {
						cout << "���������������������루y/n��:";
					}
				}
			}
		}
		//����û����ѱ�ʹ�ã�����ʾ�Ƿ���������
		else
		{
			int again = 1;
			cout << "�û����Ѵ��ڣ�" << endl << "�Ƿ��������루y/n��?" << endl;
			while (again)
			{
				string choice; cin >> choice; strip(choice, ' ');
				if (choice == "y" || choice == "Y") {
					again = 0;
				}
				else if (choice == "n" || choice == "N")
				{
					again = 0;
					return;
				}
				else {
					cout << "���������������������루y/n��:";
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
//����������ʼ���棩
int main() {
	system("color 0E");
	while (true) {
		cout << "****************��ӭ����Q&X�ļ�ϵͳ****************" << endl;
		cout << "----------------------------------------------------" << endl;
		cout << "****************************************************" << endl << endl;
		cout << "  1����¼   2��ע��     3.�û��б�      0���˳�	" << endl << endl;
		cout << "****************************************************" << endl;
		cout << "�����룺";
		int choice; cin >> choice;
		switch (choice)
		{
		case 1:Login(); break;
		case 2:Register(); getchar(); break;
		case 3:searchUser(); getchar(); break;
		case 0:return 0;
		default:cout << "���������������������룡" << endl; break;
		}
		cout << "\n\n\n�밴�س�������......" << endl;
		getchar();
		system("cls");

	}
}


