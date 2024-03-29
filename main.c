#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <intrin.h>

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <vssym32.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shobjidl_core.h>
#include <shlguid.h>
#include <commdlg.h>
#include <wincodec.h>
#include <shlwapi.h>
#pragma comment (lib, "kernel32.lib")
#pragma comment (lib, "shell32.lib")
#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "uuid.lib")
#pragma comment (lib, "comdlg32.lib")
#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "windowscodecs.lib")
#pragma comment (lib, "shlwapi.lib")

#include <GL/gl.h>
#include "res.h"

#undef near
#undef far
#define CLAMP(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : (a))
#define COUNT(arr) (sizeof(arr)/sizeof(*arr))
#define SWAP(temp,a,b) (temp)=(a); (a)=(b); (b)=temp
#define RGBA(r,g,b,a) (r | (g<<8) | (b<<16) | (a<<24))
#define RED(c) (c & 0xff)
#define GREEN(c) ((c>>8) & 0xff)
#define BLUE(c) ((c>>16) & 0xff)
#define ALPHA(c) ((c>>24) & 0xff)
#define REDF(c) (RED(c)/255.0f)
#define BLUEF(c) (BLUE(c)/255.0f)
#define GREENF(c) (GREEN(c)/255.0f)
#define ALPHAF(c) (ALPHA(c)/255.0f)

typedef struct {
	size_t len;
	char *ptr;
} Bytes;

void FatalErrorA(char *format, ...){
	va_list args;
	va_start(args,format);
	static char msg[1024];
	vsprintf(msg,format,args);
#if _DEBUG
	OutputDebugStringA(msg);
	__debugbreak();
#else
	MessageBoxA(0,msg,"Error",MB_ICONERROR);
#endif
	va_end(args);
	exit(1);
}
void FatalErrorW(WCHAR *format, ...){
	va_list args;
	va_start(args,format);
	static WCHAR msg[1024];
	vswprintf(msg,COUNT(msg),format,args);
#if _DEBUG
	OutputDebugStringW(msg);
	__debugbreak();
#else
	MessageBoxW(0,msg,L"Error",MB_ICONERROR);
#endif
	va_end(args);
	exit(1);
}
#define FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#if _DEBUG
#define ASSERT(cnd)\
	do {\
		if (!(cnd)){\
			__debugbreak();\
			exit(1);\
		}\
	} while (0)
#else
#define ASSERT(cnd)\
	do {\
		if (!(cnd)){\
			FatalErrorA(FILENAME,__LINE__,#cnd);\
			exit(1);\
		}\
	} while (0)
#endif

void *MallocOrDie(size_t size){
	void *p = malloc(size);
	ASSERT(p);
	return p;
}
void *ZallocOrDie(size_t size){
	void *p = calloc(1,size);
	ASSERT(p);
	return p;
}
void *ReallocOrDie(void *ptr, size_t size){
	void *p = realloc(ptr,size);
	ASSERT(p);
	return p;
}

/*
randint
From: https://stackoverflow.com/a/822361
Generates uniform random integers in range [0,n).
*/
int randint(int n){
	if ((n - 1) == RAND_MAX){
		return rand();
	} else {
		// Chop off all of the values that would cause skew...
		int end = RAND_MAX / n; // truncate skew
		end *= n;
		// ... and ignore results from rand() that fall above that limit.
		// (Worst case the loop condition should succeed 50% of the time,
		// so we can expect to bail out of this loop pretty quickly.)
		int r;
		while ((r = rand()) >= end);
		return r % n;
	}
}

void StringToLower(size_t len, char *str){
	for (size_t i = 0; i < len; i++){
		str[i] = tolower(str[i]);
	}
}

size_t fnv1a(size_t keylen, char *key){
	size_t index = 2166136261u;
	for (size_t i = 0; i < keylen; i++){
		index ^= key[i];
		index *= 16777619;
	}
	return index;
}

/******************* Files */
char *LoadFileA(char *path, size_t *size){
	FILE *f = fopen(path,"rb");
	if (!f) FatalErrorA("File not found: %s",path);
	ASSERT(f);
	fseek(f,0,SEEK_END);
	*size = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buf = MallocOrDie(*size);
	fread(buf,*size,1,f);
	fclose(f);
	return buf;
}
char *LoadFileW(WCHAR *path, size_t *size){
	FILE *f = _wfopen(path,L"rb");
	if (!f) FatalErrorW(L"File not found: %s",path);
	ASSERT(f);
	fseek(f,0,SEEK_END);
	*size = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buf = MallocOrDie(*size);
	fread(buf,*size,1,f);
	fclose(f);
	return buf;
}

/***************************** Image */
typedef unsigned char Color[4];

typedef struct {
	int width, height;
	Color *pixels;
} Image;

void LoadImageFromFile(Image *img, WCHAR *path, bool flip){
	static IWICImagingFactory2 *ifactory = 0;
	if (!ifactory){
		ASSERT(SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory2,0,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory2,&ifactory)));
		ASSERT(ifactory);
	}
	IWICBitmapDecoder *pDecoder = 0;
	if (S_OK != ifactory->lpVtbl->CreateDecoderFromFilename(ifactory,path,0,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&pDecoder)){
		FatalErrorW(L"LoadImageFromFile: file not found: %s",path);
	}
	IWICBitmapFrameDecode *pFrame = 0;
	pDecoder->lpVtbl->GetFrame(pDecoder,0,&pFrame);
	IWICBitmapSource *convertedSrc = 0;
	WICConvertBitmapSource(&GUID_WICPixelFormat32bppRGBA,(IWICBitmapSource *)pFrame,&convertedSrc);
	convertedSrc->lpVtbl->GetSize(convertedSrc,&img->width,&img->height);
	uint32_t size = img->width*img->height*sizeof(uint32_t);
	UINT rowPitch = img->width*sizeof(uint32_t);
	img->pixels = MallocOrDie(size);
	if (flip){
		IWICBitmapFlipRotator *pFlipRotator;
		ifactory->lpVtbl->CreateBitmapFlipRotator(ifactory,&pFlipRotator);
		pFlipRotator->lpVtbl->Initialize(pFlipRotator,convertedSrc,WICBitmapTransformFlipVertical);
		if (S_OK != pFlipRotator->lpVtbl->CopyPixels(pFlipRotator,0,rowPitch,size,(BYTE *)img->pixels)){
			FatalErrorW(L"LoadImageFromFile: %s CopyPixels failed",path);
		}
		pFlipRotator->lpVtbl->Release(pFlipRotator);
	} else {
		if (S_OK != convertedSrc->lpVtbl->CopyPixels(convertedSrc,0,rowPitch,size,(BYTE *)img->pixels)){
			FatalErrorW(L"LoadImageFromFile: %s CopyPixels failed",path);
		}
	}
	convertedSrc->lpVtbl->Release(convertedSrc);
	pFrame->lpVtbl->Release(pFrame);
	pDecoder->lpVtbl->Release(pDecoder);
}

typedef struct {
	BITMAPINFOHEADER    bmiHeader;
	RGBQUAD             bmiColors[4];
} BITMAPINFO_TRUECOLOR32;

typedef struct {
	int width, height;
	Color *pixels;
	HDC hdcBmp;
	HBITMAP hbmOld, hbm;
	HFONT fontOld;
} GdiImage;

void GdiImageNew(GdiImage *img, int width, int height){
	img->width = width;
	img->height = height;
	HDC hdcScreen = GetDC(0);
	img->hdcBmp = CreateCompatibleDC(hdcScreen);
	ReleaseDC(0,hdcScreen);
	BITMAPINFO_TRUECOLOR32 bmi = {
		.bmiHeader.biSize = sizeof(BITMAPINFOHEADER),
		.bmiHeader.biWidth = width,
		.bmiHeader.biHeight = height,
		.bmiHeader.biPlanes = 1,
		.bmiHeader.biCompression = BI_RGB | BI_BITFIELDS,
		.bmiHeader.biBitCount = 32,
		.bmiColors[0].rgbRed = 0xff,
		.bmiColors[1].rgbGreen = 0xff,
		.bmiColors[2].rgbBlue = 0xff,
	};
	img->hbm = CreateDIBSection(img->hdcBmp,(BITMAPINFO *)&bmi,DIB_RGB_COLORS,(void **)&img->pixels,0,0);
	ASSERT(img->hbm);
	img->hbmOld = SelectObject(img->hdcBmp,img->hbm);

	img->fontOld = 0;
}

void GdiImageDestroy(GdiImage *img){
	if (img->fontOld){
		SelectObject(img->hdcBmp,img->fontOld);
	}
	SelectObject(img->hdcBmp,img->hbmOld);
	DeleteDC(img->hdcBmp);
	DeleteObject(img->hbm);
	memset(img,0,sizeof(*img));
}

void GdiImageSetFont(GdiImage *img, HFONT font){
	HFONT old = SelectObject(img->hdcBmp,font);
	if (!img->fontOld){
		img->fontOld = old;
	}
	SetBkMode(img->hdcBmp,TRANSPARENT);
}

void GdiImageSetFontColor(GdiImage *img, uint32_t color){
	SetTextColor(img->hdcBmp,color & 0xffffff);
}

void GdiImageDrawText(GdiImage *img, WCHAR *str, int x, int y){
	ExtTextOutW(img->hdcBmp,x,y,0,0,str,(UINT)wcslen(str),0);
}

void GdiImageTextDimensions(GdiImage *img, WCHAR *str, int *width, int *height){
	RECT r = {0};
	DrawTextW(img->hdcBmp,str,(UINT)wcslen(str),&r,DT_CALCRECT|DT_NOPREFIX);
	*width = r.right-r.left;
	*height = r.bottom-r.top;
}

HFONT GetUserChosenFont(){
	LOGFONTW lf;
	CHOOSEFONTW cf = {
		.lStructSize = sizeof(cf),
		.lpLogFont = &lf,
		.Flags = CF_INITTOLOGFONTSTRUCT,
	};
	ASSERT(ChooseFontW(&cf));
	return CreateFontIndirectW(&lf);
}

HFONT GetSystemUiFont(){
	NONCLIENTMETRICSW ncm = {
		.cbSize = sizeof(ncm)
	};
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0);
	return CreateFontIndirectW(&ncm.lfCaptionFont);
}

GLenum glCheckError_(const char *file, int line){
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR){
		char *error;
		switch (errorCode){
			case GL_INVALID_ENUM:      error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE:     error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
			case GL_OUT_OF_MEMORY:     error = "OUT_OF_MEMORY"; break;
			default: error = "UNKNOWN TYPE BEAT";break;
		}
		FatalErrorA("%s %s (%d)",error,file,line);
	}
	return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

typedef struct {
	GLuint id;
	int width, height;
} Texture;

void TextureFromImage(Texture *t, Image *i, bool interpolated){
	t->width = i->width;
	t->height = i->height;
	if (!t->id) glGenTextures(1,&t->id);
	glBindTexture(GL_TEXTURE_2D,t->id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,interpolated ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,interpolated ? GL_LINEAR : GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t->width,t->height,0,GL_RGBA,GL_UNSIGNED_BYTE,i->pixels);
}

void TextureFromGdiImage(Texture *t, GdiImage *i, bool interpolated){
	Image img = {
		.width = i->width,
		.height = i->height,
		.pixels = i->pixels,
	};
	TextureFromImage(t,&img,interpolated);
}

void TextureFromFile(Texture *t, WCHAR *path, bool interpolated){
	Image img;
	LoadImageFromFile(&img,path,true);
	TextureFromImage(t,&img,interpolated);
	free(img.pixels);
}

void DestroyTexture(Texture *t){
	glDeleteTextures(1,&t->id);
	memset(t,0,sizeof(*t));
}

/****************** Image Processing */
/*void GreyScale(Image *img){
	for (int i = 0; i < img->width*img->height; i++){
		uint8_t *p = img->pixels+i;
		int grey = min(255,0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]);
		p[0] = grey;
		p[1] = grey;
		p[2] = grey;
	}
}
void GaussianBlur(Image *img, int strength){
	float *kernel = MallocOrDie(strength*sizeof(*kernel));
	float disx = 0.0f;
	for (int i = 0; i < strength; i++){
		kernel[i] = expf(-0.5f*disx*disx)/sqrtf(2.0f*M_PI); //This is the gaussian distribution with mean=0, standard_deviation=1
		disx += 3.0f / (strength-1);						//it happens to pretty much reach zero at x=3, so we divide that range by strength-1 for a step interval.
	}
	float sum = 0.0f;
	for (int i = 0; i < strength; i++){
		sum += (i ? 2 : 1) * kernel[i]; //Our kernel is half of a full odd dimensional kernel. The first element is the center, the other elements have to be counted twice.
	}
	for (int i = 0; i < strength; i++){
		kernel[i] /= sum; //This is the normalization step
	}
	Image b;
	b.width = img->width;
	b.height = img->height;
	b.pixels = MallocOrDie(b.width*b.height*sizeof(*b.pixels));
	for (int y = 0; y < img->height; y++){
		for (int x = 0; x < img->width; x++){
			float sums[3] = {0,0,0};
			for (int dx = -strength+1; dx < strength-1; dx++){
				uint8_t *p = img->pixels+y*img->width+CLAMP(x+dx,0,img->width-1);
				for (int i = 0; i < 3; i++){
					sums[i] = min(1.0f,sums[i]+(p[i]/255.0f)*kernel[abs(dx)]);
				}
			}
			uint8_t *p = b.pixels+y*b.width+x;
			for (int i = 0; i < 3; i++){
				p[i] = sums[i]*255;
			}
			p[3] = ((uint8_t *)(img->pixels+y*img->width+x))[3];
		}
	}
	for (int x = 0; x < img->width; x++){
		for (int y = 0; y < img->height; y++){
			float sums[3] = {0,0,0};
			for (int dy = -strength+1; dy < strength-1; dy++){
				uint8_t *p = b.pixels+CLAMP(y+dy,0,b.height-1)*b.width+x;
				for (int i = 0; i < 3; i++){
					sums[i] = min(1.0f,sums[i]+(p[i]/255.0f)*kernel[abs(dy)]);
				}
			}
			uint8_t *p = img->pixels+y*img->width+x;
			for (int i = 0; i < 3; i++){
				p[i] = sums[i]*255;
			}
			p[3] = ((uint8_t *)(b.pixels+y*b.width+x))[3];
		}
	}
	free(kernel);
	free(b.pixels);
}
void Quantize(Image *img, int divisions){
	int mins[3] = {255,255,255};
	int maxes[3] = {0,0,0};
	for (int i = 0; i < img->width*img->height; i++){
		uint8_t *p = img->pixels+i;
		for (int j = 0; j < 3; j++){
			if (p[j] < mins[j]) mins[j] = p[j];
			else if (p[j] > maxes[j]) maxes[j] = p[j];
		}
	}
	int *invals = MallocOrDie(divisions*3*sizeof(*invals));
	int *outvals = MallocOrDie(divisions*sizeof(*invals));
	int ids[3];
	for (int i = 0; i < 3; i++){
		ids[i] = (maxes[i]-mins[i])/divisions;
	}
	int od = 255/divisions;
	int ov = 0;
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < divisions; j++){
			invals[i*divisions+j] = mins[i];
			mins[i] += ids[i];
		}
	}
	for (int i = 0; i < divisions; i++){
		outvals[i] = ov;
		ov += od;
	}
	for (int i = 0; i < 3; i++){
		invals[i*divisions+divisions-1] = maxes[i];
	}
	outvals[divisions-1] = 255;

	for (int i = 0; i < img->width*img->height; i++){
		uint8_t *p = img->pixels+i;
		for (int j = 0; j < 3; j++){
			for (int k = 1; k < divisions; k++){
				if (p[j] <= invals[j*divisions+k]){
					if (p[j]-invals[j*divisions+k-1] > invals[j*divisions+k]-p[j]){
						p[j] = outvals[k];
					} else {
						p[j] = outvals[k-1];
					}
					break;
				}
			}
		}
	}

	free(invals);
	free(outvals);
}*/

/*typedef struct {
	RECT rect;
	uint32_t color;
} ColorRect;
void RectangleDecompose(ColorRectList *list, Image *img, int minDim){
	LIST_FREE(list);
	//well we need some way to mark taken rectangles
	//alpha at the moment, idk. For jpg it's clearly getting filled in with 255s
	//we're just gonna set alpha to zero for taken rectangles
	for (int y = 0; y < img->height; y++){
		for (int x = 0; x < img->width; x++){
			uint32_t c = img->pixels[y*img->width+x];
			if (ALPHA(c)){
				int i, j;
				int lim = img->width;
				for (j = y; j < img->height; j++){
					for (i = x; i < lim; i++){
						if (img->pixels[j*img->width+i] != c){
							if (lim != img->width && i!=lim){
								i = lim;
								goto L0;
							} else {
								lim = i;
							}
						}
					}
				}
			L0:;
				int width = i-x;
				int height = j-y;
				if (width >= minDim && height >= minDim){
					ColorRect *r = ColorRectListMakeRoom(list,1);
					r->rect.left = x;
					r->rect.right = i;
					r->rect.bottom = y;
					r->rect.top = j;
					r->color = c;
					list->used++;

					for (int b = y; b < j; b++){
						for (int a = x; a < i; a++){
							img->pixels[b*img->width+a] = 0;
						}
					}
				}
			}
		}
	}
	memset(img->pixels,0,img->width*img->height*sizeof(*img->pixels));
	for (ColorRect *r = list->elements; r < list->elements+list->used; r++){
		uint32_t color = randint(255) | (randint(255)<<8) | (randint(255)<<16) | (255<<24);
		for (int y = r->rect.bottom; y < r->rect.top; y++){
			for (int x = r->rect.left; x < r->rect.right; x++){
				img->pixels[y*img->width+x] = color;
			}
		}
	}
}*/

/********************** DarkWolken */
WCHAR gpath[MAX_PATH+16];
HWND gwnd;
HDC hdc;
int clientWidth = 800, clientHeight = 600;
float ortho[16];

bool greyscale = false;
int gaussianBlurStrength = 9;
int quantizeDivisions = 4;
int rectangleDecomposeMinDim = 25;

HCURSOR cursorArrow, cursorFinger, cursorPan;
int scale = 1;
bool interpolation = false;
float pos[3];
bool pan = false;
POINT panPoint;
float originalPos[3];
WCHAR imagePath[MAX_PATH];
int imagePathLen;
WCHAR textPath[MAX_PATH];
int textPathLen;
Bytes gtext;

typedef struct {
	char *prev, *cur, *end;
} Lexer;
void GetWord(Lexer *l, Bytes *w){
	while (1){
		if (l->cur == l->end || *l->cur == '\r' || *l->cur == '\n'){
			w->len = 0;
			w->ptr = 0;
			return;
		}
		if (IsCharAlphaNumericA(*l->cur)) break;
		l->cur++;
	}
	l->prev = l->cur;
	while (l->cur != l->end && IsCharAlphaNumericA(*l->cur)) l->cur++;
	w->len = l->cur-l->prev;
	w->ptr = l->prev;
}
void AdvanceLine(Lexer *l){
	while (l->cur != l->end && !(*l->cur == '\r' || *l->cur == '\n')) l->cur++;
	while (l->cur != l->end && !IsCharAlphaNumericA(*l->cur)) l->cur++;
}

typedef float vec3[3];

vec3 cube_verts[] = {
	0,1,0, 0,0,0, 0,0,1, 0,0,1, 0,1,1, 0,1,0,
	1,1,1, 1,0,1, 1,0,0, 1,0,0, 1,1,0, 1,1,1,

	0,0,0, 1,0,0, 1,0,1, 1,0,1, 0,0,1, 0,0,0,
	1,1,0, 0,1,0, 0,1,1, 0,1,1, 1,1,1, 1,1,0,

	1,1,0, 1,0,0, 0,0,0, 0,0,0, 0,1,0, 1,1,0,
	0,1,1, 0,0,1, 1,0,1, 1,0,1, 1,1,1, 0,1,1,
};

void DrawCube(){
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < 6; i++){
		vec3 *face = cube_verts+i*6;
		glTexCoord2f(0,1);
		glVertex3fv((GLfloat *)(face+0));
		glTexCoord2f(0,0);
		glVertex3fv((GLfloat *)(face+1));
		glTexCoord2f(1,0);
		glVertex3fv((GLfloat *)(face+2));

		glTexCoord2f(1,0);
		glVertex3fv((GLfloat *)(face+3));
		glTexCoord2f(1,1);
		glVertex3fv((GLfloat *)(face+4));
		glTexCoord2f(0,1);
		glVertex3fv((GLfloat *)(face+5));
	}
	glEnd();
}

HFONT sysUiFont;

Texture cobble;

int uiY = 0;
GdiImage uiImage;
typedef struct {
	WCHAR *str;
	int x, y;
} DrawTextCommand;
typedef struct {
	int total, used;
	DrawTextCommand *elements;
} DrawTextCommandList;
DrawTextCommand *DrawTextCommandListMakeRoom(DrawTextCommandList *list, int count){
	if (list->used+count > list->total){
		if (!list->total) list->total = 1;
		while (list->used+count > list->total) list->total *= 2;
		list->elements = ReallocOrDie(list->elements,list->total*sizeof(*list->elements));
	}
	list->used += count;
	return list->elements+list->used-count;
}
DrawTextCommandList uiCmdList;
void PushUiText(WCHAR *str, int x, int y){
	DrawTextCommand *dtc = DrawTextCommandListMakeRoom(&uiCmdList,1);
	dtc->str = str;
	dtc->x = x;
	dtc->y = y;
}
POINT cursorPos;
bool buttonHovered;
bool justClicked;
bool openImageClicked;
void BeginUi(){
	GdiImageNew(&uiImage,clientWidth,clientHeight);
	GdiImageSetFont(&uiImage,sysUiFont);
	GdiImageSetFontColor(&uiImage,RGB(255,255,255));

	GetCursorPos(&cursorPos);
	ScreenToClient(gwnd,&cursorPos);
	buttonHovered = false;

	uiY = 1;
}
bool Button(WCHAR *name){
	int width, height;
	GdiImageTextDimensions(&uiImage,name,&width,&height);
	width += 9;
	height += 5;

	PushUiText(name,4,uiY+2);

	glBegin(GL_TRIANGLES);

	glColor3f(1,0,0);
	glVertex3f(0,(float)uiY,0);
	glColor3f(0,1,0);
	glVertex3f(0,(float)uiY+(float)height,0);
	glColor3f(0,0,1);
	glVertex3f((float)width,(float)uiY+(float)height,0);
	
	glColor3f(0,0,1);
	glVertex3f((float)width,(float)uiY+(float)height,0);
	glColor3f(0,1,0);
	glVertex3f((float)width,(float)uiY,0);
	glColor3f(1,0,0);
	glVertex3f(0,(float)uiY,0);

	glEnd();

	RECT r = {0,uiY,width,uiY+height};
	bool thisButtonHovered = false;
	if (PtInRect(&r,cursorPos)){
		buttonHovered = true;
		thisButtonHovered = true;

		glBegin(GL_LINES);

		float hi[] = {1,1,1};
		float lo[] = {0.5,0.5,0.5};
		glColor3fv(hi);
		glVertex3f(1,(float)uiY,1);
		glColor3fv(lo);
		glVertex3f(1,(float)uiY+(float)height,1);
		glColor3fv(lo);
		glVertex3f(1,(float)uiY+(float)height,1);
		glColor3fv(lo);
		glVertex3f((float)width,(float)uiY+(float)height,1);
		glColor3fv(lo);
		glVertex3f((float)width,(float)uiY+(float)height,1);
		glColor3fv(hi);
		glVertex3f((float)width,(float)uiY,0);
		glColor3fv(hi);
		glVertex3f((float)width,(float)uiY,0);
		glColor3fv(hi);
		glVertex3f(0,(float)uiY,1);

		glEnd();
	}

	uiY += height + 4;

	return thisButtonHovered && justClicked;
}
typedef struct {float arr[16];} mat4;
mat4 mat4Persp(float fovRadians, float aspectRatio, float near, float far){
	float s = 1.0f / tanf(fovRadians * 0.5f);
	float d = near - far;
	return (mat4){
		s/aspectRatio,0,0,0,
		0,s,0,0,
		0,0,(far+near)/d,-1,
		0,0,(2*far*near)/d,0
	};
}
void EndUi(){
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glBindTexture(GL_TEXTURE_2D,cobble.id);
	glPushMatrix();
	glLoadMatrixf(mat4Persp(0.5f*(float)M_PI,(float)clientWidth/(float)clientHeight,0.01f,100.0f).arr);
	glTranslatef(0,0,-5);
	static float degrees = 0.0f;
	degrees += 1.0f;
	glRotatef(degrees,1.0f,1.0f,0.0f);
	glScalef(2.0f,2.0f,2.0f);
	glTranslatef(-0.5f,-0.5f,-0.5f);
	DrawCube();
	glPopMatrix();

	PushUiText(L"bruh",clientWidth/2,clientHeight/2-20);

	glReadPixels(0,0,clientWidth,clientHeight,GL_RGBA,GL_UNSIGNED_BYTE,(GLvoid *)uiImage.pixels);
	for (DrawTextCommand *cmd = uiCmdList.elements; cmd < uiCmdList.elements+uiCmdList.used; cmd++){
		GdiImageDrawText(&uiImage,cmd->str,cmd->x,cmd->y);
	}
	GdiFlush();
	for (Color *p = uiImage.pixels; p < uiImage.pixels+uiImage.width*uiImage.height; p++){
		p[0][3] = 0xff;
	}
	Texture tex = {0};
	TextureFromGdiImage(&tex,&uiImage,false);

	glBindTexture(GL_TEXTURE_2D,tex.id);

	glBegin(GL_TRIANGLES);

	glTexCoord2f(0,1);
	glVertex3f(0,0,1);
	glTexCoord2f(0,0);
	glVertex3f(0,(float)clientHeight,1);
	glTexCoord2f(1,0);
	glVertex3f((float)clientWidth,(float)clientHeight,1);

	glTexCoord2f(1,0);
	glVertex3f((float)clientWidth,(float)clientHeight,1);
	glTexCoord2f(1,1);
	glVertex3f((float)clientWidth,0,1);
	glTexCoord2f(0,1);
	glVertex3f(0,0,1);

	glEnd();

	glDisable(GL_TEXTURE_2D);

	DestroyTexture(&tex);
	GdiImageDestroy(&uiImage);
	if (uiCmdList.elements){
		free(uiCmdList.elements);
		memset(&uiCmdList,0,sizeof(uiCmdList));
	}

	justClicked = false;
}

LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	switch (uMsg){
		case WM_CREATE:{
			DragAcceptFiles(hwnd,1);
			gwnd = hwnd;

			//This is a hack to prevent the single white frame when the window first appears.
			//It also causes a WM_SIZE to be sent before the first WM_PAINT, so we can resize
			//our viewport and ortho matrix in the WM_SIZE handler instead of in WM_PAINT.
			RECT wr;
			GetWindowRect(hwnd,&wr);
			SetWindowPos(hwnd,0,wr.left,wr.top,wr.right-wr.left,wr.bottom-wr.top,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);
			break;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_SIZE:{
			clientWidth = LOWORD(lParam);
			clientHeight = HIWORD(lParam);
			if (clientWidth && clientHeight){
				glViewport(0,0,clientWidth,clientHeight);
				glLoadIdentity();
				glOrtho(0,clientWidth,clientHeight,0,-10,10);
			}
			return 0;
		}
		case WM_MOUSEMOVE:{
			int x = GET_X_LPARAM(lParam);
			int y = clientHeight - GET_Y_LPARAM(lParam) - 1;
			if (pan){
				pos[0] = originalPos[0] + (x - panPoint.x);
				pos[1] = originalPos[1] + (y - panPoint.y);
				InvalidateRect(hwnd,0,0);
			}
			/*for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
				if (PointInButton(b->x,clientHeight-1+b->y,b->halfWidth,b->halfHeight,x,y)){
					SetCursor(cursorFinger);
					return 0;
				}
			}*/
			SetCursor(scale == 1 ? cursorArrow : cursorPan);
			return 0;
		}
		case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:{
			int x = GET_X_LPARAM(lParam);
			int y = clientHeight - GET_Y_LPARAM(lParam) - 1;
			/*for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
				if (PointInButton(b->x,clientHeight-1+b->y,b->halfWidth,b->halfHeight,x,y)){
					b->func();
					return 0;
				}
			}*/
			if (y < clientHeight && scale > 1){
				panPoint.x = x;
				panPoint.y = y;
				memcpy(originalPos,pos,sizeof(pos));
				pan = true;
			}
			return 0;
		}
		case WM_LBUTTONUP:{
			pan = false;
			justClicked = true;
			return 0;
		}
		case WM_CHAR:{
			return 0;
		}
		case WM_KEYDOWN:{
			if (!(HIWORD(lParam) & KF_REPEAT)) switch (wParam){

			}
			return 0;
		}
		case WM_MOUSEWHEEL:{
			int oldScale = scale;
			scale = max(1,scale+CLAMP(GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA,-1,1));
			if (oldScale != scale){
				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hwnd,&pt);
				pt.y = clientHeight - pt.y - 1;
				pt.x -= (LONG)pos[0];
				pt.y -= (LONG)pos[1];
				if (scale > oldScale){
					pos[0] -= pt.x / (float)(scale-1);
					pos[1] -= pt.y / (float)(scale-1);
				} else {
					pos[0] += pt.x / (float)(scale+1);
					pos[1] += pt.y / (float)(scale+1);
				}
				InvalidateRect(hwnd,0,0);
				if (GetCursor() != cursorFinger) SetCursor(scale == 1 ? cursorArrow : cursorPan);
			}
			return 0;
		}
		case WM_PAINT:{
			if (!clientWidth || !clientHeight){
				return 0;
			}
			glCheckError();

			glClearColor(0.122f,0.122f,0.137f,1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			BeginUi();
			if (Button(L"Open Image")){
				openImageClicked = true;
				ValidateRect(hwnd,0);
				//this scheme is required because the open file dialogs bug the hell
				//out if the main window keeps trying to repaint itself.
			}
			if (Button(L"Open Text")){
				printf("Open Text\n");
			}
			EndUi();

			SwapBuffers(hdc);

			return 0;
		}
	}
	return DefWindowProcW(hwnd,uMsg,wParam,lParam);
}
#if _DEBUG
void main(){
#else
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd){
#endif
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	ASSERT(SUCCEEDED(CoInitialize(0)));

	WNDCLASSEXW wcex = {
		.cbSize = sizeof(wcex),
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		.lpfnWndProc = WndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = GetModuleHandleW(0),
		.hIcon = LoadIconW(GetModuleHandleW(0),MAKEINTRESOURCEW(RID_ICON)),
		.hCursor = 0,
		.hbrBackground = NULL,
		.lpszMenuName = NULL,
		.lpszClassName = L"WordCloud",
		.hIconSm = NULL,
	};

	ASSERT(RegisterClassExW(&wcex));

	RECT rect = {0,0,640,480};
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	ASSERT(AdjustWindowRect(&rect,style,false));
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	HWND hwnd = CreateWindowExW(
		0,
		wcex.lpszClassName,
		wcex.lpszClassName,
		style,
		GetSystemMetrics(SM_CXSCREEN)/2-width/2,
		GetSystemMetrics(SM_CYSCREEN)/2-height/2,
		width,
		height,
		0,
		0,
		wcex.hInstance,
		0);
	ASSERT(hwnd);

	hdc = GetDC(hwnd);
	ASSERT(hdc);

	PIXELFORMATDESCRIPTOR pfd = {0};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;

	int pixelFormat = ChoosePixelFormat(hdc,&pfd);
	ASSERT(pixelFormat);
	ASSERT(SetPixelFormat(hdc,pixelFormat,&pfd));
	HGLRC hglrc = wglCreateContext(hdc);
	ASSERT(hglrc);
	ASSERT(wglMakeCurrent(hdc,hglrc));

	typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	ASSERT(wglSwapIntervalEXT);
	wglSwapIntervalEXT(1); //enable vsync

	cursorArrow = LoadCursorW(0,IDC_ARROW);
	cursorFinger = LoadCursorW(0,IDC_HAND);
	cursorPan = LoadCursorW(0,IDC_SIZEALL);
	SetCursor(cursorArrow);

	int argc;
	WCHAR **argv = CommandLineToArgvW(GetCommandLineW(),&argc);
	if (argc==2){
		//LoadImg(argv[1]);
	}

	TextureFromFile(&cobble,L"cobble.png",false);

	sysUiFont = GetSystemUiFont();

	ShowWindow(hwnd,SW_SHOW);

	MSG msg;
	while (GetMessageW(&msg,0,0,0)){
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		if (openImageClicked){
			openImageClicked = false;

			IFileDialog *pfd;
			IShellItem *psi;
			PWSTR path = 0;
			COMDLG_FILTERSPEC fs = {L"Image files", L"*.png;*.jpg;*.jpeg"};
			ASSERT(SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog,0,CLSCTX_ALL,&IID_IFileOpenDialog,&pfd)));
			pfd->lpVtbl->SetFileTypes(pfd,1,&fs); 
			pfd->lpVtbl->SetTitle(pfd,L"Open Image");
			pfd->lpVtbl->Show(pfd,hwnd);
			if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd,&psi))){
				if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi,SIGDN_FILESYSPATH,&path))){

					CoTaskMemFree(path);
				}
				psi->lpVtbl->Release(psi);
			}
			pfd->lpVtbl->Release(pfd);

			InvalidateRect(hwnd,0,false);
		}
	}

	CoUninitialize();
}