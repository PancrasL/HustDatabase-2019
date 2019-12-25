#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBase.h"
#include "HustBaseDoc.h"
#include <iostream>
#include <vector>
#include <set>
#include <string>
using std::set;
using std::string;
bool isOpened = false;//标识数据库是否打开

/*自定义函数*/
//将字符串转换为小写
void toLowerString(std::string &s);
//在界面显示一条消息
void showMsg(CEditArea *editArea, char * msg);
//输出select的查询结果
void showSelectResult(SelResult &res, CEditArea *editArea);


void ExecuteAndMessage(char * sql, CEditArea* editArea) {//根据执行的语句类型在界面上显示执行结果。此函数需修改
	std::string s_sql = sql;
	RC rc;
	toLowerString(s_sql);
	//查询语句的处理
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		rc = Query(sql, &res);
		if (rc == SUCCESS) {
			showSelectResult(res, editArea);
			return;
		}
	}
	//其他语句的处理
	else {
		//执行
		rc = execute(sql);

		//更新界面
		CHustBaseDoc *pDoc;
		pDoc = CHustBaseDoc::GetDoc();
		CHustBaseApp::pathvalue = true;
		pDoc->m_pTreeView->PopulateTree();
	}

	//执行结果
	switch (rc) {
	case SUCCESS:
		showMsg(editArea, "操作成功");
		break;
	case SQL_SYNTAX:
		showMsg(editArea, "有语法错误");
		break;
	case TABLE_NOT_EXIST:
		showMsg(editArea, "表不存在");
		break;
	case PF_FILEERR:
		showMsg(editArea, "文件不存在");
		break;
	case TABLE_EXIST:
		showMsg(editArea, "表已存在，无法重复创建");
		break;
	case TABLE_COLUMN_ERROR:
		showMsg(editArea, "属性名过长或属性同名");
	default:
		showMsg(editArea, "功能未实现");
		break;
	}
}

RC execute(char * sql) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//只有两种返回结果SUCCESS和SQL_SYNTAX

	if (rc != SUCCESS)
		return rc;

	switch (sql_str->flag)
	{
		//case 1:
		////判断SQL语句为select语句

		//break;

	case 2:
		//判断SQL语句为insert语句
		rc = Insert(sql_str->sstr.ins.relName, sql_str->sstr.ins.nValues, sql_str->sstr.ins.values);
		break;
	case 3:
		//判断SQL语句为update语句
		rc = Update(sql_str->sstr.upd.relName, sql_str->sstr.upd.attrName, &(sql_str->sstr.upd.value), sql_str->sstr.upd.nConditions, sql_str->sstr.upd.conditions);
		break;

	case 4:
		//判断SQL语句为delete语句
		rc = Delete(sql_str->sstr.del.relName, sql_str->sstr.del.nConditions, sql_str->sstr.del.conditions);
		break;

	case 5:
		//判断SQL语句为createTable语句
		rc = CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
		break;

	case 6:
		//判断SQL语句为dropTable语句
		rc = DropTable(sql_str->sstr.drt.relName);
		break;

	case 7:
		//判断SQL语句为createIndex语句
		rc = CreateIndex(sql_str->sstr.crei.indexName, sql_str->sstr.crei.relName, sql_str->sstr.crei.attrName);
		break;

	case 8:
		//判断SQL语句为dropIndex语句
		rc = DropIndex(sql_str->sstr.dri.indexName);
		break;

	case 9:
		//判断为help语句，可以给出帮助提示
		break;

	case 10:
		//判断为exit语句，可以由此进行退出操作
		break;
	}
	return rc;
}

RC CreateDB(char *dbpath, char *dbname) {
	//在dbpath下创建dbname文件夹
	CString path = dbpath;
	path += "\\";
	path += dbname;
	CreateDirectory(path, NULL);

	//在dbname文件夹下创建表文件和列文件
	SetCurrentDirectory(path);
	RC rc;
	rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	if (rc != SUCCESS)
		return rc;
	rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumn));
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropDB(char *dbname) {
	BOOL ret = TRUE;
	CFileFind finder;
	CString path;
	path.Format(_T("%s/*"), dbname);
	BOOL bWorking = finder.FindFile(path);
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory() && !finder.IsDots())
		{//处理文件夹
			char subDir[200];
			std::string dir = finder.GetFilePath();
			sprintf_s(subDir, "%s", dir.c_str());
			DropDB(subDir); //递归删除文件夹
			RemoveDirectory(finder.GetFilePath());
		}
		else
		{//处理文件
			DeleteFile(finder.GetFilePath());
		}
	}
	return SUCCESS;
}

RC OpenDB(char *dbname) {
	isOpened = true;
	return SUCCESS;
}

RC CloseDB() {
	isOpened = false;
	return SUCCESS;
}

/*
建表操作：
1.初始化系统表文件和系统列文件
2.创建数据表文件
*/
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle rm_table, rm_column;
	RID rid;
	int recordsize;//数据表的每条记录的大小
	AttrInfo * attrtmp = attributes;
	RM_FileScan FileScan;
	RM_Record rectab;
	int attrcount;

	//表存在
	if (_access(relName, 0) == 0)
		return TABLE_EXIST;

	//判断表明是否过长
	if (strlen(relName) > 20)
		return TABLE_NAME_ILLEGAL;

	//判断属性名是否符合要求
	set<string> s;
	for (int i = 0; i < attrCount; i++) {
		if (s.find(attributes[i].attrName) != s.end())//存在同名属性
			return TABLE_COLUMN_ERROR;
		s.insert(attributes[i].attrName);
		if (strlen(attributes[i].attrName) > 10)
			return TABLE_COLUMN_ERROR;
	}
	/*打开系统表文件并更新*/
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	pData = new char[sizeof(SysTable)];
	memcpy(pData, relName, 21);
	memcpy(pData + 21, &attrCount, sizeof(int));
	rid.bValid = false;
	rc = InsertRec(&rm_table, pData, &rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	//释放申请的内存空间
	delete[] pData;

	/*打开列文件并更新*/
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;
	for (int i = 0, offset = 0; i < attrCount; i++, attrtmp++) {
		pData = new char[sizeof(SysColumn)];
		memcpy(pData, relName, 21);
		memcpy(pData + 21, attrtmp->attrName, 21);
		memcpy(pData + 42, &(attrtmp->attrType), sizeof(AttrType));
		memcpy(pData + 42 + sizeof(AttrType), &(attrtmp->attrLength), sizeof(int));
		memcpy(pData + 42 + sizeof(int) + sizeof(AttrType), &offset, sizeof(int));
		memcpy(pData + 42 + 2 * sizeof(int) + sizeof(AttrType), "0", sizeof(char));
		rid.bValid = false;
		rc = InsertRec(&rm_column, pData, &rid);
		if (rc != SUCCESS) {
			return rc;
		}
		delete[] pData;
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS) {
		return rc;
	}

	/*创建数据表*/

	//计算recordsize的大小
	recordsize = 0;
	attrtmp = attributes;
	for (int i = 0; i < attrCount; i++, attrtmp++) {
		recordsize += attrtmp->attrLength;
	}
	rc = RM_CreateFile(relName, recordsize);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

/*
删表操作
1.把表名为relName的表删除
2.将数据库中该表对应的信息置空
*/
RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	//删除不不存在的表
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	/*删除数据表文件*/
	tmp.Remove((LPCTSTR)relName);

	/*将系统表和系统列中对应表的相关记录删除*/
	//打开表文件和列文件
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;

	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			DeleteRec(&rm_table, &(rectab.rid));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	for (int i = 0; i < attrcount && GetNextRec(&FileScan, &reccol) == SUCCESS;) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			DeleteRec(&rm_column, &(reccol.rid));
			i++;
		}
		delete[] reccol.pData;
	}

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC CreateIndex(char *indexName, char *relName, char *attrName) {
	return SUCCESS;
}

RC DropIndex(char *indexName) {
	return SUCCESS;
}

RC Insert(char *relName, int nValues, Value *values) {
	RM_FileHandle rm_data, rm_table, rm_column;
	char *value;//读取数据表信息
	RID rid;
	RC rc;
	SysColumn *column, *ctmp;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	//判断表是否存在
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	//打开数据表,系统表，系统列文件
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS) {
		//AfxMessageBox("打开数据表文件失败");
		return rc;
	}
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS) {
		//AfxMessageBox("打开系统表文件失败");
		return rc;
	}
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS) {
		//AfxMessageBox("打开系统列文件失败");
		return rc;
	}
	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS) {
		//AfxMessageBox("初始化文件扫描失败");
		return rc;
	}
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete rectab.pData;
	}

	//判断列数与表定义不一致
	if (attrcount != nValues) {
		return FIELD_MISSING;
	}
	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS) {
		//AfxMessageBox("初始化数据列文件扫描失败");
		return rc;
	}
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	column = new SysColumn[attrcount]();
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++, ctmp++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
		delete[] reccol.pData;
	}
	ctmp = column;

	//判断数据类型是否与表一致
	for (int i = 0; i < nValues; i++) {
		if (values[nValues - 1 - i].type != column[i].attrType)
			return FIELD_TYPE_MISMATCH;
	}

	//向数据表中循环插入记录
	value = new char[rm_data.fileSubHeader->recordSize];
	values = values + nValues - 1;
	for (int i = 0; i < nValues; i++, values--, ctmp++) {
		memcpy(value + ctmp->attrOffset, values->data, ctmp->attrLength);
	}
	rid.bValid = false;
	InsertRec(&rm_data, value, &rid);
	delete[] value;

	delete[] column;

	//关闭文件句柄
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS) {
		//AfxMessageBox("关闭数据表失败");
		return rc;
	}
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS) {
		//AfxMessageBox("关闭系统表失败");
		return rc;
	}
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS) {
		//AfxMessageBox("关闭系统列失败");
		return rc;
	}
	return SUCCESS;
}

RC Delete(char *relName, int nConditions, Condition *conditions) {
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否符合删除条件
	int attrcount;//临时 属性数量
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//临时 属性的值
	AttrType attrtype;

	//判断表是否存在
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	//打开数据表,系统表，系统列文件
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				ctmp++;
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//判断列是否存在
	for (int i = 0; i < nConditions; i++) {
		if (conditions[i].bLhsIsAttr) {
			if (s.find(conditions[i].lhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
		if (conditions[i].bRhsIsAttr) {
			if (s.find(conditions[i].rhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
	}

	//通过getdata函数获取系统表信息,得到的信息保存在recdata中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的数据表中的记录,并将记录信息保存于recdata中
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {	//取记录做判断
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions条件逐一判断
			ctmpleft = ctmpright = column;//每次循环都要将遍历整个系统列文件，找到各个条件对应的属性
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpleft->attrType) {//判定属性的类型
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
					break;
				}
			}
			//右属性左值
			if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpright->attrType) {
				case ints:
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			//左右均属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpright->attrType) {
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}

		if (torf == 1) {
			DeleteRec(&rm_data, &(recdata.rid));
		}
		delete[] recdata.pData;
	}
	delete[] column;

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC Update(char *relName, char *attrName, Value *Value, int nConditions, Condition *conditions) {
	//只能进行单值更新
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *cupdate, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否符合删除条件
	int attrcount;//临时 属性数量
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//临时 属性的值
	AttrType attrtype;

	//表不存在
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;
	//打开数据表,系统表，系统列文件
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				if ((strcmp(relName, ctmp->tabName) == 0) && (strcmp(attrName, ctmp->attrName) == 0)) {
					cupdate = ctmp;//找到要更新数据 对应的属性
				}
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
				ctmp++;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//列不存在
	if (s.find(attrName) == s.end())
		return TABLE_COLUMN_ERROR;
	
	//通过getdata函数获取系统表信息,得到的信息保存在recdata中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的数据表中的记录,并将记录信息保存于recdata中
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions条件逐一判断
			ctmpleft = ctmpright = column;//每次循环都要将遍历整个系统列文件，找到各个条件对应的属性
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpleft->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
				}
				else if (ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
				}
				else if (ctmpleft->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
				}
				else
					torf &= 0;
			}
			//右属性左值
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpright->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			//左右均属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpright->attrType == ints && ctmpleft->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars && ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats && ctmpleft->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}
		if (torf == 1) {
			memcpy(recdata.pData + cupdate->attrOffset, Value->data, cupdate->attrLength);
			UpdateRec(&rm_data, &recdata);
		}
		delete[] recdata.pData;
	}

	delete[] column;

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

bool CanButtonClick() {//需要重新实现
	return isOpened;
}

void toLowerString(std::string & s)
{
	for (unsigned i = 0; i < s.length(); i++)
	{
		s[i] = tolower(s[i]);
	}
}

void showMsg(CEditArea * editArea, char * msg)
{
	int row_num = 1;
	char **messages = new char*[row_num];
	messages[0] = msg;
	editArea->ShowMessage(row_num, messages);
	delete[] messages;
}

void showSelectResult(SelResult & res, CEditArea * editArea)
{
	/*输出执行结果*/
	char ** fields = new char*[res.col_num];
	for (int i = 0; i < res.col_num; i++) {
		fields[i] = new char[20];
		memcpy(fields[i], res.fields[i], 20);
	}
	editArea->ShowSelResult(res.col_num, res.row_num, fields, res.res);

	/*释放动态分配的空间*/
	for (int i = 0; i < res.col_num; i++) {
		delete[] fields[i];
	}
	delete[] fields;

	Destory_Result(&res);
}
