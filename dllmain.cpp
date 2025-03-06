// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

//プラグインのバージョン文字列(既定)
const char* STR_VERSION = "00IN";
//このプラグインの説明文字列（名前、作者あたり）
const char* STR_ABOUT = "STL ver 1.0";

//対応する拡張指数
const int EXT_TYPES = 2;
//拡張子のリスト「"*.jpg"」などワイルドカードの書式で、対応するものを羅列
const char* STR_EXTS[EXT_TYPES] = { "*.stl", "*.model"};
//対応拡張子の詳細説明の羅列
const char* STR_EXTS_ABOUT[EXT_TYPES] = { "STL", "model in 3mf"};

//modelファイル判定用の文字列(雑)
const char* STR_XMLHEAD = "<?xml ";
const int STR_XMLHEAD_LEN = 6;

//描画サイズ
#define	RENDER_W	1536
#define	RENDER_H	1024

//既定のエラーコード
#define OK              0  /* 正常終了 */
#define NOT_SUPPORTED  -1  /* その機能はインプリメントされていない */
#define CANCELED        1  /* コールバック関数が非0を返したので展開を中止した */
#define UNKNOWN         2  /* 未知のフォーマット */
#define BROKEN          3  /* データが壊れている */
#define NO_MEMORY       4  /* メモリーが確保出来ない */
#define NO_FILE         6  /* ファイルリードエラー */
#define SOME_ERROR      8  /* 内部エラー */

//メモリアライメントの現在の設定を保存
#pragma pack(push)
//メモリアライメントを1byteに設定
//ワード単位に設定されていると hInfoの位置がずれる
#pragma pack(1)

//画像情報の構造体
struct PictureInfo
{
	long left, top;    /*画像を展開する位置*/
	long width;       /*画像の幅(pixel)*/
	long height;      /*画像の高さ(pixel)*/
	WORD x_density;   /*画素の水平方向密度*/
	WORD y_density;   /*画素の垂直方向密度*/
	short colorDepth; /*１画素当たりのbit数*/
	HLOCAL hInfo;     /*画像内のテキスト情報*/
};

//アライメントを戻す
#pragma pack(pop)



//プラグインの基礎情報を要求してくる関数
//infonoで欲しい文字列を指定してくるのでbufにbuflenを気にして入れてあげる
//
// infono 	取得する情報番号
//		情報番号 	意味
//		0 	Plug - in APIバージョン
//		1 	Plug - in名、バージョン及びcopyright
//		SusieのAbout..に表示されます
//		2n + 2 	代表的な拡張子("*.JPG" "*.RGB;*.Q0"など)
//		2n + 3 	ファイル形式名
//
//	buf 	情報を納めるバッファ
//	buflen 	バッファ長(byte)
//	戻り値	バッファに書き込んだ文字数を返します。情報番号が無効の場合0を返します。 

extern "C" __declspec(dllexport) int PASCAL GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	const char* s = nullptr;

	//バージョン文字列
	if (infono == 0)
	{
		s = STR_VERSION;
	}
	//説明文字列
	else if (infono == 1)
	{
		s = STR_ABOUT;
	}
	//対応拡張子、その説明
	else if(infono >= 2)
	{
		int idx = (infono - 2) / 2;
		if (idx < EXT_TYPES)
		{
			//0,2,4...が拡張子
			if (!(infono & 1))
			{
				s = STR_EXTS[idx];
			}
			//1,3,5...が説明
			else
			{
				s = STR_EXTS_ABOUT[idx];
			}
		}
	}
	//対象があったら処理
	if (s)
	{
		size_t len = strlen(s);
		strncpy_s(buf, buflen, s, len);
		return len;
	}
	//なかったらなかったことを通知
	return 0;
}


//ファイルがサポートしてる要件を満たしているかのチェックを要求してくる関数
//ファイルヘッダチェック的な感じ
//dwが特殊で、ファイルハンドルか、メモリポインタが渡されてくる。
//ハンドルの場合は、そのハンドルでファイルを読んでチェック、メモリの場合はそのメモリをチェック
//メモリの場合が多いらしい上に、2K程度のようなのでファイル全体でのチェックが必要などのファイルだとやりにくいかも。
//
//filename	ファイル名
//dw
//	上位ワードが0
//		ファイルハンドル
//	上位ワードが非0
//		ファイル先頭部(2Kbyte以上)を読み込んだバッファへのポインタ。 ファイルサイズが2Kbyte以下の場合もバッファは2Kbyte確保し、余分は0で埋めること。
//戻り値	対応している画像フォーマットであれば非0を返す。 そうでない場合は0を返す。

extern "C" __declspec(dllexport) int PASCAL IsSupported(LPSTR filename, DWORD dw)
{
	int ret = OK;
	char* data = nullptr;

	//ファイルハンドル
	if ((dw & 0xFFFF0000) == 0) {
		LPDWORD lpReadBytes = 0;
		data = (char*)calloc(1, 100);
		if (ReadFile((HANDLE)dw, data, 100, lpReadBytes, NULL) == 0)
		{
			ret = NO_FILE;
		}
	}
	//メモリポインタ
	else {
		data = (char*)dw;
	}

	//ヘッダチェック
	//マジックナンバー的なのがないので、80bytesのコメント部分でバイナリが出ないかチェック
	//文字コード次第で無駄かもしれない
	if (data)
	{
		if (!strncmp(data, STR_XMLHEAD, STR_XMLHEAD_LEN))
		{
			//modelファイルと過信する
		}
		else
		{
			for (int i = 0; i < 80; ++i)
			{
				char c = data[i];
				if (c >= 0x7F)
				{
					ret = UNKNOWN;
					break;
				}
			}
		}
	}

	//メモリ確保していたら解放
	if(data != nullptr) free(data);

	return ret;
}


//実際に画像の情報を要求してくる関数
//	buf 	IN
//		入力がファイルの場合 	ファイル名
//		入力がメモリーの場合 	ファイルイメージへのポインタ
//	len 	IN
//		入力がファイルの場合 	読み込み開始オフセット(MacBin対応のため)
//		入力がメモリーの場合 	データサイズ
//	flag 	IN 	追加情報 「xxxx xxxx xxxx xSSS」(ビットフラグとして見る)
//		SSSは入力の種類を意味する
//			0 	ディスクファイル
//			1 	メモリ上のイメージ
//	lpInfo 	OUT 	画像情報
//	戻り値	0なら正常終了、それ以外はエラーコードを返す

extern "C" __declspec(dllexport) int PASCAL GetPictureInfo(LPSTR buf, long len, unsigned int flag, struct PictureInfo* lpInfo)
{
	//チェックと情報生成
	lpInfo->left = 0;
	lpInfo->top = 0;
	lpInfo->width = RENDER_W;
	lpInfo->height = RENDER_H;
	lpInfo->x_density = 0;
	lpInfo->y_density = 0;
	lpInfo->colorDepth = 24;
	lpInfo->hInfo = NULL;

	return OK;
}


//実際の画像データを要求してくる関数
//	buf 	IN
//		入力がファイルの場合 	ファイル名
//		入力がメモリーの場合 	ファイルイメージへのポインタ
//	len 	IN
//		入力がファイルの場合 	読み込み開始オフセット(MacBin対応のため)
//		入力がメモリーの場合 	データサイズ
//	flag 	IN 	追加情報 「xxxx xxxx xxxx xSSS」(ビットフラグとして見る)
//		SSSは入力の種類を意味する
//			0 	ディスクファイル
//			1 	メモリ上のイメージ
//	pHBInfo 	OUT 	BITMAPINFO構造体が納められたメモリハンドルが返される
//	pHBm 	OUT 	ビットマップデータ本体のメモリハンドルが返される
//	lpPrgressCallback 	IN 	途中経過を表示するコールバック関数へのポインタ。 NULLの場合、Plug - inは処理が終了するまでプロセスを占有し、中断も出来ません。
//	lData 	IN 	コールバック関数に渡すlongデータ。 ポインタなどを必要に応じて受け渡せる。
//	戻り値	0なら正常終了、それ以外はエラーコードを返す
//
//bufで指定された画像を展開する。 プラグインはLocalAllocによって必要なメモリーを確保し、そのハンドルを返す。
//アプリケーションはLocalFreeによってメモリーを開放する必要がある。
//関数が成功した場合にpHBInfoとpHBmのメモリハンドルで確保されている領域を解放するのは呼び出し側の責任である。
//またpHBInfo、pHBmで返されるメモリハンドルで確保される領域はアンロックされている。
//つまりPlug - in内でメモり領域に対して書き込みをしたあとはLocalUnlockした後に呼び出し側へ戻る。 
/*
 HLOCAL *pHBInfo,HLOCAL *pHBm,
 はポインタのポインタ
*/

#include "stl.h"

extern "C" __declspec(dllexport) int PASCAL GetPicture(
	LPSTR buf, long len, unsigned int flag, 
	HLOCAL* pHBInfo, HLOCAL* pHBm,
	int (CALLBACK* lpPrgressCallback)(int, int, long), long lData)
{
	char* data = nullptr;
	bool file_flag = false;

	//ファイル
	if ((flag & 7) == 0)
	{
		FILE* fp;
		int f_length;
		if(fopen_s(&fp, buf, "rb") != 0) return NO_FILE;
		f_length = _filelength(_fileno(fp));
		data = (char*)malloc(f_length);
		if (data == nullptr)
		{
			return NO_MEMORY;
		}
		fread(data, 1, f_length, fp);
		fclose(fp);
		file_flag = true;
	}
	//メモリ
	else {
		data = buf;
	}

	//受け渡すためのメモリを確保
	*pHBInfo = LocalAlloc(LMEM_FIXED, sizeof(BITMAPINFO));
	*pHBm = LocalAlloc(LMEM_FIXED, RENDER_W * RENDER_H * 3);
	if (*pHBInfo == nullptr) return NO_MEMORY;
	if (*pHBm == nullptr) return NO_MEMORY;

	//基礎情報を入れてしまう
	BITMAPINFO* Info = (BITMAPINFO*)*pHBInfo;
	Info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info->bmiHeader.biWidth = RENDER_W;
	Info->bmiHeader.biHeight = RENDER_H;
	Info->bmiHeader.biPlanes = 1;
	Info->bmiHeader.biBitCount = 24;
	Info->bmiHeader.biCompression = BI_RGB;
	Info->bmiHeader.biSizeImage = RENDER_W * RENDER_H;
	Info->bmiHeader.biXPelsPerMeter = 0;
	Info->bmiHeader.biYPelsPerMeter = 0;
	Info->bmiHeader.biClrUsed = 0;
	Info->bmiHeader.biClrImportant = 0;

	//レンダリング
	if (!strncmp(data, STR_XMLHEAD, STR_XMLHEAD_LEN))
	{
		//modelファイルと過信する
		Model* mdl = new Model();
		mdl->Render(data, (char*)*pHBm, RENDER_W, RENDER_H);
	}
	else
	{
		Stl* stl = new Stl();
		stl->Render(data, (char*)*pHBm, RENDER_W, RENDER_H);
	}
	return OK;
}


//サムネイルを要求してくる関数
//実装は任意で、実装していなければ-1を返せばよい
//	buf 	IN
//		入力がファイルの場合 	ファイル名
//		入力がメモリーの場合 	ファイルイメージへのポインタ
//	len 	IN
//		入力がファイルの場合 	読み込み開始オフセット(MacBin対応のため)
//		入力がメモリーの場合 	データサイズ
//	flag 	IN 	追加情報 「xxxx xxxx xxxx xSSS」(ビットフラグとして見る)
//		SSSは入力の種類を意味する
//			0 	ディスクファイル
//			1 	メモリ上のイメージ
//	pHBInfo 	OUT 	BITMAPINFO構造体が納められたメモリハンドルが返される
//	pHBm 	OUT 	ビットマップデータ本体のメモリハンドルが返される
//	lpPrgressCallback 	IN 	途中経過を表示するコールバック関数へのポインタ。 NULLの場合、Plug - inは処理が終了するまでプロセスを占有し、中断も出来ません。
//	lData 	IN 	コールバック関数に渡すlongデータ。 ポインタなどを必要に応じて受け渡せる。
//	戻り値	0なら正常終了、それ以外はエラーコードを返す。 この関数はオプションであり、未対応の場合には - 1を返す。

extern "C" __declspec(dllexport) int PASCAL GetPreview(LPSTR buf, long len, unsigned int flag, HANDLE* pHBInfo, HANDLE* pHBm, FARPROC lpPrgressCallback, long lData)
{
	return NOT_SUPPORTED;
}

