int _fltused;
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <intrin.h>
#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
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
#pragma comment (lib, "dwmapi.lib")
#pragma comment (lib, "comdlg32.lib")
#pragma comment (lib, "uxtheme.lib")
#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "windowscodecs.lib")
#pragma comment (lib, "shlwapi.lib")

#include <GL/gl.h>
#include "mgl.h"
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

void FatalErrorA(char *format, ...){
	va_list args;
	va_start(args,format);
#if _DEBUG
	vprintf(format,args);
	printf("\n");
#else
	char msg[512];
	vsprintf(msg,format,args);
	MessageBoxA(0,msg,"Error",MB_ICONEXCLAMATION);
#endif
	va_end(args);
	exit(1);
}
void FatalErrorW(WCHAR *format, ...){
	va_list args;
	va_start(args,format);
#if _DEBUG
	vwprintf(format,args);
	wprintf(L"\n");
#else
	WCHAR msg[512];
	vswprintf(msg,COUNT(msg),format,args);
	MessageBoxW(0,msg,L"Error",MB_ICONEXCLAMATION);
#endif
	va_end(args);
	exit(1);
}

void *MallocOrDie(size_t size){
	void *p = malloc(size);
	Assert(p);
	if (!p) FatalErrorA("malloc failed.");
	return p;
}
void *ZallocOrDie(size_t size){
	void *p = calloc(1,size);
	if (!p) FatalErrorA("calloc failed.");
	return p;
}
void *ReallocOrDie(void *ptr, size_t size){
	void *p = realloc(ptr,size);
	if (!p) FatalErrorA("realloc failed.");
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

#define LIST_INIT(list)\
	(list)->total = 0;\
	(list)->used = 0;\
	(list)->elements = 0;
#define LIST_FREE(list)\
	if ((list)->total){\
		free((list)->elements);\
		(list)->total = 0;\
		(list)->used = 0;\
		(list)->elements = 0;\
	}
#define LIST_IMPLEMENTATION(type)\
typedef struct type##List {\
	size_t total, used;\
	type *elements;\
} type##List;\
type *type##ListMakeRoom(type ## List *list, size_t count){\
	if (list->used+count > list->total){\
		if (!list->total) list->total = 1;\
		while (list->used+count > list->total) list->total *= 2;\
		list->elements = ReallocOrDie(list->elements,list->total*sizeof(type));\
	}\
	return list->elements+list->used;\
}\
void type##ListMakeRoomAtIndex(type ## List *list, size_t index, size_t count){\
	type##ListMakeRoom(list,count);\
	if (index != list->used) memmove(list->elements+index+count,list->elements+index,(list->used-index)*sizeof(type));\
}\
void type##ListInsert(type ## List *list, size_t index, type *elements, size_t count){\
	type##ListMakeRoomAtIndex(list,index,count);\
	memcpy(list->elements+index,elements,count*sizeof(type));\
	list->used += count;\
}\
void type##ListAppend(type ## List *list, type *elements, size_t count){\
	type##ListMakeRoom(list,count);\
	memcpy(list->elements+list->used,elements,count*sizeof(type));\
	list->used += count;\
}

size_t fnv1a(size_t keylen, char *key){
	size_t index = 2166136261u;
	for (size_t i = 0; i < keylen; i++){
		index ^= key[i];
		index *= 16777619;
	}
	return index;
}

#define LINKED_HASHLIST_INIT(list)\
	(list)->total = 0;\
	(list)->used = 0;\
	(list)->buckets = 0;\
	(list)->first = 0;\
	(list)->last = 0;
#define LINKED_HASHLIST_FREE(list)\
	if ((list)->total){\
		free((list)->buckets);\
		LINKED_HASHLIST_INIT(list);\
	}
#define LINKED_HASHLIST_IMPLEMENTATION(type)\
typedef struct type##LinkedHashListBucket {\
	struct type##LinkedHashListBucket *prev, *next;\
	size_t keylen;\
	char *key;\
	type value;\
} type##LinkedHashListBucket;\
typedef struct type##LinkedHashList {\
	size_t total, used;\
	type##LinkedHashListBucket *buckets, *first, *last;\
} type##LinkedHashList;\
type##LinkedHashListBucket *type##LinkedHashListGetBucket(type##LinkedHashList *list, size_t keylen, char *key){\
	if (!list->total) return 0;\
	size_t index = fnv1a(keylen,key);\
	index %= list->total;\
	type##LinkedHashListBucket *lastTombstone = 0;\
	while (1){\
		type##LinkedHashListBucket *bucket = list->buckets+index;\
		if (bucket->keylen < 0) lastTombstone = bucket;\
		else if (!bucket->keylen) return lastTombstone ? lastTombstone : bucket;\
		else if (bucket->keylen == keylen && !memcmp(bucket->key,key,keylen)) return bucket;\
		index = (index + 1) % list->total;\
	}\
}\
type *type##LinkedHashListGet(type##LinkedHashList *list, size_t keylen, char *key){\
	type##LinkedHashListBucket *bucket = type##LinkedHashListGetBucket(list,keylen,key);\
	if (bucket && bucket->keylen > 0) return &bucket->value;\
	else return 0;\
}\
type *type##LinkedHashListNew(type##LinkedHashList *list, size_t keylen, char *key){\
	if (!list->total){\
		list->total = 8;\
		list->buckets = ZallocOrDie(8*sizeof(*list->buckets));\
	}\
	if (list->used+1 > (list->total*3)/4){\
		type##LinkedHashList newList;\
		newList.total = list->total * 2;\
		newList.used = list->used;\
		newList.buckets = ZallocOrDie(newList.total*sizeof(*newList.buckets));\
		for (type##LinkedHashListBucket *bucket = list->first; bucket;){\
			type##LinkedHashListBucket *newBucket = type##LinkedHashListGetBucket(&newList,bucket->keylen,bucket->key);\
			newBucket->keylen = bucket->keylen;\
			newBucket->key = bucket->key;\
			newBucket->value = bucket->value;\
			newBucket->next = 0;\
			if (bucket == list->first){\
				newBucket->prev = 0;\
				newList.first = newBucket;\
				newList.last = newBucket;\
			} else {\
				newBucket->prev = newList.last;\
				newList.last->next = newBucket;\
				newList.last = newBucket;\
			}\
			bucket = bucket->next;\
		}\
		free(list->buckets);\
		*list = newList;\
	}\
	type##LinkedHashListBucket *bucket = type##LinkedHashListGetBucket(list,keylen,key);\
	if (bucket->keylen <= 0){\
		list->used++;\
		bucket->keylen = keylen;\
		bucket->key = key;\
		bucket->next = 0;\
		if (list->last){\
			bucket->prev = list->last;\
			list->last->next = bucket;\
			list->last = bucket;\
		} else {\
			bucket->prev = 0;\
			list->first = bucket;\
			list->last = bucket;\
		}\
	}\
	return &bucket->value;\
}\
void type##LinkedHashListRemove(type##LinkedHashList *list, size_t keylen, char *key){\
	type##LinkedHashListBucket *bucket = type##LinkedHashListGetBucket(list,keylen,key);\
	if (bucket->keylen > 0){\
		if (bucket->prev){\
			if (bucket->next){\
				bucket->prev->next = bucket->next;\
				bucket->next->prev = bucket->prev;\
			} else {\
				bucket->prev->next = 0;\
				list->last = bucket->prev;\
			}\
		} else if (bucket->next){\
			list->first = bucket->next;\
			bucket->next->prev = 0;\
		}\
		bucket->keylen = -1;\
		list->used--;\
	}\
}

/***************************** RingBuffer */
/*void RingBufferReset(RingBuffer *r){
r->used = 0;
r->writeIndex = 0;
r->readIndex = 0;
}
void RingBufferInit(RingBuffer *r, size_t elementSize, int total, void *elements){
r->elementSize = elementSize;
r->total = total;
r->elements = elements;
RingBufferReset(r);
}
void* RingBufferGet(RingBuffer *r, int index){
return ((char *)r->elements)+((r->readIndex+index)%r->total)*r->elementSize;
}
void *RingBufferIncrementWriteHead(RingBuffer *r){
if (r->used == r->total) return 0;
void *w = ((char *)r->elements)+(r->writeIndex%r->total)*r->elementSize;
r->writeIndex = (r->writeIndex + 1) % r->total;
r->used++;
return w;
}
void RingBufferIncrementReadHead(RingBuffer *r){
if (!r->used) return;
r->readIndex = (r->readIndex + 1) % r->total;
r->used--;
}*/

/***************************** Linear Algebra */
#define LERP(a,b,t) ((a) + (t)*((b)-(a)))
inline float Float3Dot(float a[3], float b[3]){
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
inline float Float3Length(float a[3]){
	return sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}
inline float Float3AngleBetween(float a[3], float b[3]){
	return acosf((a[0]*b[0] + a[1]*b[1] + a[2]*b[2])/(sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2])*sqrtf(b[0]*b[0] + b[1]*b[1] + b[2]*b[2])));
}
inline void Float3Cross(float a[3], float b[3], float out[3]){
	out[0] = a[1]*b[2] - a[2]*b[1];
	out[1] = a[2]*b[0] - a[0]*b[2];
	out[2] = a[0]*b[1] - a[1]*b[0];
}
inline void Float3Add(float a[3], float b[3]){
	for (int i = 0; i < 3; i++) a[i] += b[i];
}
inline void Float3Subtract(float a[3], float b[3]){
	for (int i = 0; i < 3; i++) a[i] -= b[i];
}
inline void Float3Negate(float a[3]){
	for (int i = 0; i < 3; i++) a[i] = -a[i];
}
inline void Float3Scale(float a[3], float scale){
	for (int i = 0; i < 3; i++) a[i] = a[i]*scale;
}
inline void Float3Normalize(float a[3]){
	float d = 1.0f / sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
	for (int i = 0; i < 3; i++) a[i] *= d;
}
inline void Float3SetLength(float a[3], float length){
	float d = length / sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
	for (int i = 0; i < 3; i++) a[i] *= d;
}
inline void Float3Midpoint(float a[3], float b[3], float out[3]){
	for (int i = 0; i < 3; i++) out[i] = a[i] + 0.5f*(b[i]-a[i]);
}
inline void Float3Lerp(float a[3], float b[3], float t){
	for (int i = 0; i < 3; i++) a[i] = LERP(a[i],b[i],t);
}
inline void ClampEuler(float e[3]){
	for (int i = 0; i < 3; i++){
		if (e[i] > 4.0f*M_PI) e[i] -= 4.0f*M_PI;
		else if (e[i] < -4.0f*M_PI) e[i] += 4.0f*M_PI;
	}
}
inline void QuaternionSlerp(float a[4], float b[4], float t){//from https://github.com/microsoft/referencesource/blob/51cf7850defa8a17d815b4700b67116e3fa283c2/System.Numerics/System/Numerics/Quaternion.cs#L289C8-L289C8
	float cosOmega = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
	bool flip = false;
	if (cosOmega < 0.0f){
		flip = true;
		cosOmega = -cosOmega;
	}
	float s1,s2;
	if (cosOmega > (1.0f - 1e-6f)){
		s1 = 1.0f - t;
		s2 = flip ? -t : t;
	} else {
		float omega = acosf(cosOmega);
		float invSinOmega = 1.0f / sinf(omega);
		s1 = sinf((1.0f-t)*omega)*invSinOmega;
		float sm = sinf(t*omega)*invSinOmega;
		s2 = flip ? -sm : sm;
	}
	for (int i = 0; i < 4; i++) a[i] = s1*a[i] + s2*b[i];
}
inline void Float4x4Identity(float a[16]){
	a[0] = 1.0f;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = 1.0f;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = 1.0f;
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4FromBasisVectors(float a[16], float x[3], float y[3], float z[3]){
	a[0] = x[0];
	a[1] = x[1];
	a[2] = x[2];
	a[3] = 0.0f;

	a[4] = y[0];
	a[5] = y[1];
	a[6] = y[2];
	a[7] = 0.0f;

	a[8] = z[0];
	a[9] = z[1];
	a[10] = z[2];
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4Transpose(float a[16]){
	/*
	0 4 8  12
	1 5 9  13
	2 6 10 14
	3 7 11 15
	*/
	float t;
	SWAP(t,a[1],a[4]);
	SWAP(t,a[2],a[8]);
	SWAP(t,a[3],a[12]);
	SWAP(t,a[6],a[9]);
	SWAP(t,a[7],a[13]);
	SWAP(t,a[11],a[14]);
}
inline void Float4x4TransposeMat3(float a[16]){
	/*
	0 4 8  12
	1 5 9  13
	2 6 10 14
	3 7 11 15
	*/
	float t;
	SWAP(t,a[1],a[4]);
	SWAP(t,a[2],a[8]);
	SWAP(t,a[6],a[9]);
}
inline void Float4x4Orthogonal(float a[16], float left, float right, float bottom, float top, float near, float far){
	a[0] = 2.0f/(right-left);
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = 2.0f/(top-bottom);
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = 2.0f/(near-far);
	a[11] = 0.0f;

	a[12] = (right+left)/(left-right);
	a[13] = (top+bottom)/(bottom-top);
	a[14] = (far+near)/(near-far);
	a[15] = 1.0f;
}
inline void Float4x4Perspective(float a[16], float fovRadians, float aspectRatio, float near, float far){
	float s = 1.0f / tanf(fovRadians * 0.5f);
	float d = near - far;

	a[0] = s/aspectRatio;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = s;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = (far+near)/d;
	a[11] = -1.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = (2.0f*far*near)/d;
	a[15] = 0.0f;
}
inline void Float4x4Scale(float a[16], float x, float y, float z){
	a[0] = x;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = y;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = z;
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4ScaleV(float a[16], float scale[3]){
	a[0] = scale[0];
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = scale[1];
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = scale[2];
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4Translation(float a[16], float x, float y, float z){
	a[0] = 1.0f;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = 1.0f;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = 1.0f;
	a[11] = 0.0f;

	a[12] = x;
	a[13] = y;
	a[14] = z;
	a[15] = 1.0f;
}
inline void Float4x4TranslationV(float a[16], float translation[3]){
	a[0] = 1.0f;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = 1.0f;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = 1.0f;
	a[11] = 0.0f;

	a[12] = translation[0];
	a[13] = translation[1];
	a[14] = translation[2];
	a[15] = 1.0f;
}
inline void Float4x4RotationX(float a[16], float angle){
	float c = cosf(angle);
	float s = sinf(angle);

	a[0] = 1.0f;
	a[1] = 0.0f;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = c;
	a[6] = s;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = -s;
	a[10] = c;
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4RotationY(float a[16], float angle){
	float c = cosf(angle);
	float s = sinf(angle);

	a[0] = c;
	a[1] = 0.0f;
	a[2] = -s;
	a[3] = 0.0f;

	a[4] = 0.0f;
	a[5] = 1.0f;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = s;
	a[9] = 0.0f;
	a[10] = c;
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4RotationZ(float a[16], float angle){
	float c = cosf(angle);
	float s = sinf(angle);

	a[0] = c;
	a[1] = s;
	a[2] = 0.0f;
	a[3] = 0.0f;

	a[4] = -s;
	a[5] = c;
	a[6] = 0.0f;
	a[7] = 0.0f;

	a[8] = 0.0f;
	a[9] = 0.0f;
	a[10] = 1.0f;
	a[11] = 0.0f;

	a[12] = 0.0f;
	a[13] = 0.0f;
	a[14] = 0.0f;
	a[15] = 1.0f;
}
inline void Float4x4Multiply(float a[16], float b[16], float out[16]){
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			out[i*4+j] = a[j]*b[i*4] + a[j+4]*b[i*4+1] + a[j+8]*b[i*4+2] + a[j+12]*b[i*4+3];
}
inline void Float4x4MultiplyFloat4(float m[16], float v[4], float out[4]){
	for (int i = 0; i < 4; i++) out[i] = v[0]*m[i] + v[1]*m[4+i] + v[2]*m[8+i] + v[3]*m[12+i];
}
inline void Float4x4LookAtY(float m[16], float eye[3], float target[3]){//rotates eye about Y to look at target
	float kx = target[0] - eye[0];
	float kz = target[2] - eye[2];
	float d = 1.0f / sqrtf(kx*kx + kz*kz);
	kx *= d;
	kz *= d;

	m[0] = -kz;
	m[1] = 0.0f;
	m[2] = kx;
	m[3] = 0.0f;

	m[4] = 0.0f;
	m[5] = 1.0f;
	m[6] = 0.0f;
	m[7] = 0.0f;

	m[8] = -kx;
	m[9] = 0.0f;
	m[10] = -kz;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}
inline void Float4x4LookAtXY(float m[16], float eye[3], float target[3]){//rotates eye about X and Y to look at target
	float kx = target[0] - eye[0],
		ky = target[1] - eye[1],
		kz = target[2] - eye[2];
	float d = 1.0f / sqrtf(kx*kx + ky*ky + kz*kz);
	kx *= d;
	ky *= d;
	kz *= d;

	float ix = -kz,
		iz = kx;
	d = 1.0f / sqrtf(ix*ix + iz*iz);
	ix *= d;
	iz *= d;

	float jx = -iz*ky,
		jy = iz*kx - ix*kz,
		jz = ix*ky;

	m[0] = ix;
	m[1] = 0.0f;
	m[2] = iz;
	m[3] = 0.0f;

	m[4] = jx;
	m[5] = jy;
	m[6] = jz;
	m[7] = 0.0f;

	m[8] = -kx;
	m[9] = -ky;
	m[10] = -kz;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}
inline void Float4x4LookAtXYView(float m[16], float eye[3], float target[3]){//produces a view matrix
	float kx = target[0] - eye[0],
		ky = target[1] - eye[1],
		kz = target[2] - eye[2];
	float d = 1.0f / sqrtf(kx*kx + ky*ky + kz*kz);
	kx *= d;
	ky *= d;
	kz *= d;

	float ix = -kz,
		iz = kx;
	d = 1.0f / sqrtf(ix*ix + iz*iz);
	ix *= d;
	iz *= d;

	float jx = -iz*ky,
		jy = iz*kx - ix*kz,
		jz = ix*ky;

	m[0] = ix;
	m[1] = jx;
	m[2] = -kx;
	m[3] = 0.0f;

	m[4] = 0.0f;
	m[5] = jy;
	m[6] = -ky;
	m[7] = 0.0f;

	m[8] = iz;
	m[9] = jz;
	m[10] = -kz;
	m[11] = 0.0f;

	m[12] = -(ix*eye[0] + iz*eye[2]);
	m[13] = -(jx*eye[0] + jy*eye[1] + jz*eye[2]);
	m[14] = kx*eye[0] + ky*eye[1] + kz*eye[2];
	m[15] = 1.0f;
}
/*
inline void Float4x4LookAtXYViewShake(Float4x4 *m, Float3 *eye, Float3 *target){
Float4x4LookAtXY(m,eye,target);

}
Float4x4 mat4LookAtShake(FVec3 eye, FVec3 target, FVec3 shakeEuler){

FVec3 z = fvec3Norm(fvec3Sub(eye,target)),
x = fvec3Norm(fvec3Cross(z,(FVec3){0,1,0})),
y = fvec3Cross(x,z);
return mat4Mul(mat4Transpose(mat4Mul(eulerToFloat4x4(shakeEuler),mat4Basis(fvec3Negate(x),y,z))),mat4Pos(fvec3Negate(eye)));
}
Float4x4 eulerToFloat4x4(FVec3 e){
return mat4Mul(mat4RotZ(e.z),mat4Mul(mat4RotY(e.y),mat4RotX(e.x)));
}
Float4x4 quatToFloat4x4(FVec4 q){
Float4x4 m = mat4Identity();
m.a0 = 1 - 2*(q.y*q.y + q.z*q.z);
m.a1 = 2*(q.x*q.y + q.z*q.w);
m.a2 = 2*(q.x*q.z - q.y*q.w);

m.b0 = 2*(q.x*q.y - q.z*q.w);
m.b1 = 1 - 2*(q.x*q.x + q.z*q.z);
m.b2 = 2*(q.y*q.z + q.x*q.w);

m.c0 = 2*(q.x*q.z + q.y*q.w);
m.c1 = 2*(q.y*q.z - q.x*q.w);
m.c2 = 1 - 2*(q.x*q.x + q.y*q.y);
return m;
}
Float4x4 mat4MtwInverse(Float4x4 m){
Float4x4 i = mat4TransposeMat3(m);
i.d.vec3 = fvec3Negate(m.d.vec3);
return i;
}
void mat4Print(Float4x4 m){
for (int i = 0; i < 4; i++){
printf("%f %f %f %f\n",m.arr[0*4+i],m.arr[1*4+i],m.arr[2*4+i],m.arr[3*4+i]);
}
}
FVec3 fvec3Rotated(FVec3 v, FVec3 euler){
return mat4MulFVec4(eulerToFloat4x4(euler),(FVec4){v.x,v.y,v.z,0.0f}).vec3;
}
FVec3 normalFromTriangle(FVec3 a, FVec3 b, FVec3 c){
return fvec3Norm(fvec3Cross(fvec3Sub(b,a),fvec3Sub(c,a)));
}
int manhattanDistance(IVec3 a, IVec3 b){
return abs(a.x-b.x)+abs(a.y-b.y)+abs(a.z-b.z);
}
Float4x4 getVP(Camera *c){
return mat4Mul(mat4Persp(c->fov,c->aspect,0.01f,1024.0f),mat4Mul(mat4Transpose(eulerToFloat4x4(c->euler)),mat4Pos(fvec3Scale(c->position,-1))));
}
void rotateCamera(Camera *c, float dx, float dy, float sens){
c->euler.y += sens * dx;
c->euler.x += sens * dy;
c->euler = clampEuler(c->euler);
}
//The following SAT implementation is modified from https://stackoverflow.com/a/52010428
bool getSeparatingPlane(FVec3 RPos, FVec3 Plane, FVec3 *aNormals, FVec3 aHalfExtents, FVec3 *bNormals, FVec3 bHalfExtents){
return (fabsf(fvec3Dot(RPos,Plane)) > 
(fabsf(fvec3Dot(fvec3Scale(aNormals[0],aHalfExtents.x),Plane)) +
fabsf(fvec3Dot(fvec3Scale(aNormals[1],aHalfExtents.y),Plane)) +
fabsf(fvec3Dot(fvec3Scale(aNormals[2],aHalfExtents.z),Plane)) +
fabsf(fvec3Dot(fvec3Scale(bNormals[0],bHalfExtents.x),Plane)) + 
fabsf(fvec3Dot(fvec3Scale(bNormals[1],bHalfExtents.y),Plane)) +
fabsf(fvec3Dot(fvec3Scale(bNormals[2],bHalfExtents.z),Plane))));
}
bool OBBsColliding(OBB *a, OBB *b){
FVec3 RPos = fvec3Sub(b->position,a->position);
Float4x4 aRot = eulerToFloat4x4(a->euler);
FVec3 aNormals[3] = {
aRot.a.vec3,
aRot.b.vec3,
aRot.c.vec3
};
Float4x4 bRot = eulerToFloat4x4(b->euler);
FVec3 bNormals[3] = {
bRot.a.vec3,
bRot.b.vec3,
bRot.c.vec3
};
return !(getSeparatingPlane(RPos,aNormals[0],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,aNormals[1],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,aNormals[2],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,bNormals[0],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,bNormals[1],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,bNormals[2],aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[0],bNormals[0]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[0],bNormals[1]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[0],bNormals[2]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[1],bNormals[0]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[1],bNormals[1]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[1],bNormals[2]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[2],bNormals[0]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[2],bNormals[1]),aNormals,a->halfExtents,bNormals,b->halfExtents) ||
getSeparatingPlane(RPos,fvec3Cross(aNormals[2],bNormals[2]),aNormals,a->halfExtents,bNormals,b->halfExtents));
}
*/

/******************* Files */
char *LoadFileA(char *path, size_t *size){
	FILE *f = fopen(path,"rb");
	if (!f) FatalErrorA("File not found: %s",path);
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
	fseek(f,0,SEEK_END);
	*size = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buf = MallocOrDie(*size);
	fread(buf,*size,1,f);
	fclose(f);
	return buf;
}
int CharIsSuitableForFileName(char c){
	return (c==' ') || ((','<=c)&&(c<='9')) || (('A'<=c)&&(c<='Z')) || (('_'<=c)&&(c<='z'));
}
BOOL FileOrFolderExists(LPCTSTR path){
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/***************************** Image */
typedef struct {
	int width,height,rowPitch;
	uint32_t *pixels;
} Image;
void LoadImageFromFile(Image *img, WCHAR *path, bool flip){
	static IWICImagingFactory2 *ifactory = 0;
	if (!ifactory && FAILED(CoCreateInstance(&CLSID_WICImagingFactory2,0,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory2,&ifactory))){
		FatalErrorW(L"LoadImageFromFile: failed to create IWICImagingFactory2");
	}
	IWICBitmapDecoder *pDecoder = 0;
	if (S_OK != ifactory->lpVtbl->CreateDecoderFromFilename(ifactory,path,0,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&pDecoder)){
		FatalErrorW(L"LoadImageFromFile: file not found: %s",path);
	}
	IWICBitmapFrameDecode *pFrame = 0;
	pDecoder->lpVtbl->GetFrame(pDecoder,0,&pFrame);
	IWICBitmapSource *convertedSrc = 0;
	WICConvertBitmapSource(&GUID_WICPixelFormat32bppRGBA,pFrame,&convertedSrc);
	convertedSrc->lpVtbl->GetSize(convertedSrc,&img->width,&img->height);
	uint32_t size = img->width*img->height*sizeof(uint32_t);
	img->rowPitch = img->width*sizeof(uint32_t);
	img->pixels = MallocOrDie(size);
	if (flip){
		IWICBitmapFlipRotator *pFlipRotator;
		ifactory->lpVtbl->CreateBitmapFlipRotator(ifactory,&pFlipRotator);
		pFlipRotator->lpVtbl->Initialize(pFlipRotator,convertedSrc,WICBitmapTransformFlipVertical);
		if (S_OK != pFlipRotator->lpVtbl->CopyPixels(pFlipRotator,0,img->rowPitch,size,img->pixels)){
			FatalErrorW(L"LoadImageFromFile: %s CopyPixels failed",path);
		}
		pFlipRotator->lpVtbl->Release(pFlipRotator);
	} else {
		if (S_OK != convertedSrc->lpVtbl->CopyPixels(convertedSrc,0,img->rowPitch,size,img->pixels)){
			FatalErrorW(L"LoadImageFromFile: %s CopyPixels failed",path);
		}
	}
	convertedSrc->lpVtbl->Release(convertedSrc);
	pFrame->lpVtbl->Release(pFrame);
	pDecoder->lpVtbl->Release(pDecoder);
}

/***************************** GLUtil */
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
#define GL_FUNCTIONS(X) \
	X(PFNGLCREATESHADERPROC,             glCreateShader             ) \
	X(PFNGLSHADERSOURCEPROC,             glShaderSource             ) \
	X(PFNGLGETSHADERIVPROC,              glGetShaderiv              ) \
	X(PFNGLGETSHADERINFOLOGPROC,         glGetShaderInfoLog         ) \
	X(PFNGLGETPROGRAMIVPROC,             glGetProgramiv             ) \
	X(PFNGLGETPROGRAMINFOLOGPROC,        glGetProgramInfoLog        ) \
	X(PFNGLCOMPILESHADERPROC,            glCompileShader            ) \
	X(PFNGLCREATEPROGRAMPROC,            glCreateProgram            ) \
	X(PFNGLATTACHSHADERPROC,             glAttachShader             ) \
	X(PFNGLLINKPROGRAMPROC,              glLinkProgram              ) \
	X(PFNGLDELETESHADERPROC,             glDeleteShader             ) \
	X(PFNGLUSEPROGRAMPROC,				 glUseProgram				) \
	X(PFNGLGETATTRIBLOCATIONPROC,        glGetAttribLocation        ) \
	X(PFNGLGETUNIFORMLOCATIONPROC,       glGetUniformLocation       ) \
	X(PFNGLGENVERTEXARRAYSPROC,          glGenVertexArrays          ) \
	X(PFNGLBINDVERTEXARRAYPROC,          glBindVertexArray          ) \
	X(PFNGLGENBUFFERSPROC,               glGenBuffers               ) \
	X(PFNGLBINDBUFFERPROC,               glBindBuffer               ) \
	X(PFNGLBUFFERDATAPROC,               glBufferData               ) \
	X(PFNGLDELETEBUFFERSPROC,            glDeleteBuffers            ) \
	X(PFNGLENABLEVERTEXATTRIBARRAYPROC,  glEnableVertexAttribArray  ) \
	X(PFNGLVERTEXATTRIBPOINTERPROC,      glVertexAttribPointer      ) \
	X(PFNGLUNIFORM1FPROC,                glUniform1f                ) \
	X(PFNGLUNIFORM2FPROC,                glUniform2f                ) \
	X(PFNGLUNIFORM3FPROC,                glUniform3f                ) \
	X(PFNGLUNIFORM4FPROC,                glUniform4f                ) \
	X(PFNGLUNIFORM1FVPROC,               glUniform1fv               ) \
	X(PFNGLUNIFORM2FVPROC,               glUniform2fv               ) \
	X(PFNGLUNIFORM3FVPROC,               glUniform3fv               ) \
	X(PFNGLUNIFORM4FVPROC,               glUniform4fv               ) \
	X(PFNGLUNIFORM1IPROC,                glUniform1i                ) \
	X(PFNGLUNIFORM2IPROC,                glUniform2i                ) \
	X(PFNGLUNIFORM3IPROC,                glUniform3i                ) \
	X(PFNGLUNIFORM4IPROC,                glUniform4i                ) \
	X(PFNGLUNIFORM1IVPROC,               glUniform1iv               ) \
	X(PFNGLUNIFORM2IVPROC,               glUniform2iv               ) \
	X(PFNGLUNIFORM3IVPROC,               glUniform3iv               ) \
	X(PFNGLUNIFORM4IVPROC,               glUniform4iv               ) \
	X(PFNGLUNIFORMMATRIX4FVPROC,         glUniformMatrix4fv         ) \
																	  \
	X(PFNGLGENFRAMEBUFFERSPROC,			 glGenFramebuffers			) \
	X(PFNGLDELETEFRAMEBUFFERSPROC,		 glDeleteFramebuffers		) \
	X(PFNGLBINDFRAMEBUFFERPROC,			 glBindFramebuffer			) \
	X(PFNGLFRAMEBUFFERTEXTURE2DPROC,	 glFramebufferTexture2D		)
#define X(type, name) type name;
GL_FUNCTIONS(X)
#undef X

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
void TextureFromFile(Texture *t, WCHAR *path, bool interpolated){
	Image img;
	LoadImageFromFile(&img,path,true);
	TextureFromImage(t,&img,interpolated);
	free(img.pixels);
}
void CheckShader(GLuint id){
	GLint result;
	glGetShaderiv(id,GL_COMPILE_STATUS,&result);
	if (!result){
		char infolog[512];
		glGetShaderInfoLog(id,512,NULL,infolog);
		FatalErrorA(infolog);
	}
}
void CheckProgram(GLuint id, GLenum param){
	GLint result;
	glGetProgramiv(id,param,&result);
	if (!result){
		char infolog[512];
		glGetProgramInfoLog(id,512,NULL,infolog);
		FatalErrorA(infolog);
	}
}
GLuint CompileShader(char *name, char *vertSrc, char *fragSrc){
	printf("Compiling %s\n",name);
	GLuint v = glCreateShader(GL_VERTEX_SHADER);
	GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(v,1,&vertSrc,NULL);
	glShaderSource(f,1,&fragSrc,NULL);
	glCompileShader(v);
	printf("\tvertex shader\n");
	CheckShader(v);
	glCompileShader(f);
	printf("\tfragment shader\n");
	CheckShader(f);
	GLuint p = glCreateProgram();
	glAttachShader(p,v);
	glAttachShader(p,f);
	glLinkProgram(p);
	CheckProgram(p,GL_LINK_STATUS);
	glDeleteShader(v);
	glDeleteShader(f);
	return p;
}

/*********** Shaders */
typedef struct {
	float Position[3];
	float Rectangle[4];
	float RoundingRadius;
	uint32_t color;
	uint32_t IconColor;
} RoundedRectangleVertex;
enum RoundedRectangleType {
	RR_CHANNEL=51,
	RR_FLAT=102,
	RR_DISH=153,
	RR_CONE=204
};
enum RoundedRectangleIconType {
	RR_ICON_NONE=36,
	RR_ICON_PLAY=72,
	RR_ICON_REVERSE_PLAY=108,
	RR_ICON_PAUSE=144,
	RR_ICON_NEXT=180,
	RR_ICON_PREVIOUS=216,
};
struct RoundedRectangleShader {
	char *vertSrc;
	char *fragSrc;
	GLuint id;
	GLint aPosition;
	GLint aRectangle;
	GLint aRoundingRadius;
	GLint aColor;
	GLint aIconColor;
	GLint proj;
} RoundedRectangleShader = {
	"#version 110\n"
	"attribute vec3 aPosition;\n"
	"attribute vec4 aRectangle;\n"//xy: center, zw: half extents
	"attribute float aRoundingRadius;\n"
	"attribute vec4 aColor;\n"
	"attribute vec4 aIconColor;\n"
	"uniform mat4 proj;\n"
	"varying vec4 Rectangle;\n"
	"varying float RoundingRadius;\n"
	"varying vec4 Color;\n"
	"varying vec4 IconColor;\n"
	"void main(){\n"
	"	gl_Position = proj * vec4(aPosition,1.0);\n"
	"	Rectangle = aRectangle;\n"
	"	RoundingRadius = aRoundingRadius;\n"
	"	Color = aColor;\n"
	"	IconColor = aIconColor;\n"
	"}",

	"#version 110\n"
	"#define PI 3.14159265\n"
	"varying vec4 Rectangle;\n"
	"varying float RoundingRadius;\n"
	"varying vec4 Color;\n"//alpha selects shading type
	"varying vec4 IconColor;\n"//alpha selects icon type
	"float DistanceAABB(vec2 p, vec2 he, float r){\n"//based on qmopey's shader: https://www.shadertoy.com/view/cts3W2
	"	vec2 d = abs(p) - he + r;\n"
	"	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;\n"
	"}\n"
	// The MIT License
	// sdEquilateralTriangle Copyright  2017 Inigo Quilez https://www.shadertoy.com/view/Xl2yDW
	// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	"float DistanceEquilateralTriangle(vec2 p, float r){\n"
	"	float k = sqrt(3.0);\n"
	"	p.x = abs(p.x) - r;\n"
	"	p.y = p.y + r/k;\n"
	"	if( p.x+k*p.y>0.0 ) p=vec2(p.x-k*p.y,-k*p.x-p.y)/2.0;\n"
	"	p.x -= clamp( p.x, -2.0*r, 0.0 );\n"
	"	if (p.y > 0.0) return -length(p);\n"
	"	else return length(p);\n"
	"}\n"
	"vec2 rotate(vec2 p, float angle){\n"
	"	float c = cos(angle);\n"
	"	float s = sin(angle);\n"
	"	mat2 m = mat2(c,-s,s,c);\n"
	"	return m * p;\n"
	"}\n"
	"void main(){\n"
	"	vec2 p = gl_FragCoord.xy-Rectangle.xy;\n"
	"	vec2 pn = p;\n"
	"	if (pn.x < 0.0) pn.x += min(abs(pn.x),Rectangle.z-RoundingRadius);\n"
	"	else pn.x -= min(pn.x,Rectangle.z-RoundingRadius);\n"
	"	float d = DistanceAABB(p,Rectangle.zw,RoundingRadius);\n"
	"	if (Color.a < 0.25){\n"//CHANNEL
	"		vec3 outerColor = Color.rgb+dot(pn/RoundingRadius,vec2(1,-1))*vec3(0.1);\n"
	"		gl_FragColor = vec4(outerColor,1.0-clamp(d,0.0,1.0));\n"
	"		return;\n"
	"	}\n"
	"	vec3 outerColor = Color.rgb+dot(pn/RoundingRadius,vec2(-1,1))*vec3(0.1);\n"
	"	float innerRadius = RoundingRadius*0.8;\n"
	"	vec3 innerColorShift;\n"
	"	vec3 innerColor;\n"
	"	if (Color.a < 0.5){\n"//FLAT
	"		innerColorShift = vec3(0.0);\n"
	"		innerColor = Color.rgb;\n"
	"	} else if (Color.a < 0.75){\n"//DISH
	"		innerColorShift = dot(pn/innerRadius,vec2(1,-1))*vec3(0.1);\n"
	"		innerColor = Color.rgb+innerColorShift;\n"
	"	} else {\n"//CONE
	"		innerColorShift = dot(normalize(pn/innerRadius),vec2(1,-1))*vec3(0.1);\n"
	"		innerColor = Color.rgb+innerColorShift;\n"
	"	}\n"
	"	if (IconColor.a < 0.1666) gl_FragColor = vec4(mix(innerColor,outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	else if (IconColor.a < 0.333){\n"
	"		vec3 iconColorShaded = IconColor.rgb+innerColorShift;\n"
	"		float iconD = DistanceEquilateralTriangle(rotate(p,-PI/2.0),RoundingRadius/4.0)-(RoundingRadius/16.0);\n"
	"		gl_FragColor = vec4(mix(mix(iconColorShaded,innerColor,clamp(iconD,0.0,1.0)),outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	} else if (IconColor.a < 0.5){\n"
	"		vec3 iconColorShaded = IconColor.rgb+innerColorShift;\n"
	"		float iconD = DistanceEquilateralTriangle(rotate(p,PI/2.0),RoundingRadius/4.0)-(RoundingRadius/16.0);\n"
	"		gl_FragColor = vec4(mix(mix(iconColorShaded,innerColor,clamp(iconD,0.0,1.0)),outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	} else if (IconColor.a < 0.666){\n"
	"		vec3 iconColorShaded = IconColor.rgb+innerColorShift;\n"
	"		vec2 halfExtents = vec2(innerRadius/9.0,innerRadius/3.0);\n"
	"		vec2 spacing = vec2(halfExtents.x*2.0,0.0);\n"
	"		float iconD = min(DistanceAABB(p+spacing,halfExtents,halfExtents.x),DistanceAABB(p-spacing,halfExtents,halfExtents.x));\n"
	"		gl_FragColor = vec4(mix(mix(iconColorShaded,innerColor,clamp(iconD,0.0,1.0)),outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	} else if (IconColor.a < 0.833){\n"
	"		vec3 iconColorShaded = IconColor.rgb+innerColorShift;\n"
	"		vec2 halfExtents = vec2(RoundingRadius/16.0,RoundingRadius/4.0);\n"
	"		float playRadius = RoundingRadius/5.0;\n"
	"		float playRoundingRadius = playRadius/4.0;\n"
	"		float iconD = min(DistanceEquilateralTriangle(rotate(p+vec2(halfExtents.x,0.0),-PI/2.0),playRadius)-playRoundingRadius,DistanceAABB(p-vec2(playRadius+playRoundingRadius-halfExtents.x,0.0),halfExtents,halfExtents.x));\n"
	"		gl_FragColor = vec4(mix(mix(iconColorShaded,innerColor,clamp(iconD,0.0,1.0)),outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	} else {\n"
	"		vec3 iconColorShaded = IconColor.rgb+innerColorShift;\n"
	"		vec2 halfExtents = vec2(RoundingRadius/16.0,RoundingRadius/4.0);\n"
	"		float playRadius = RoundingRadius/5.0;\n"
	"		float playRoundingRadius = playRadius/4.0;\n"
	"		float iconD = min(DistanceEquilateralTriangle(rotate(p-vec2(halfExtents.x,0.0),PI/2.0),playRadius)-playRoundingRadius,DistanceAABB(p+vec2(playRadius+playRoundingRadius-halfExtents.x,0.0),halfExtents,halfExtents.x));\n"
	"		gl_FragColor = vec4(mix(mix(iconColorShaded,innerColor,clamp(iconD,0.0,1.0)),outerColor,clamp(d+(RoundingRadius-innerRadius),0.0,1.0)),1.0-clamp(d,0.0,1.0));\n"
	"	}\n"
	"}\n"
};
void CompileRoundedRectangleShader(){
	RoundedRectangleShader.id = CompileShader("RoundedRectangleShader",RoundedRectangleShader.vertSrc,RoundedRectangleShader.fragSrc);
	RoundedRectangleShader.aPosition = glGetAttribLocation(RoundedRectangleShader.id,"aPosition");
	RoundedRectangleShader.aRectangle = glGetAttribLocation(RoundedRectangleShader.id,"aRectangle");
	RoundedRectangleShader.aRoundingRadius = glGetAttribLocation(RoundedRectangleShader.id,"aRoundingRadius");
	RoundedRectangleShader.aColor = glGetAttribLocation(RoundedRectangleShader.id,"aColor");
	RoundedRectangleShader.aIconColor = glGetAttribLocation(RoundedRectangleShader.id,"aIconColor");
	RoundedRectangleShader.proj = glGetUniformLocation(RoundedRectangleShader.id,"proj");
}
void RoundedRectangleShaderPrepBuffer(){
	glEnableVertexAttribArray(RoundedRectangleShader.aPosition);
	glEnableVertexAttribArray(RoundedRectangleShader.aRectangle);
	glEnableVertexAttribArray(RoundedRectangleShader.aRoundingRadius);
	glEnableVertexAttribArray(RoundedRectangleShader.aColor);
	glEnableVertexAttribArray(RoundedRectangleShader.aIconColor);
	glVertexAttribPointer(RoundedRectangleShader.aPosition,3,GL_FLOAT,GL_FALSE,sizeof(RoundedRectangleVertex),0);
	glVertexAttribPointer(RoundedRectangleShader.aRectangle,4,GL_FLOAT,GL_FALSE,sizeof(RoundedRectangleVertex),offsetof(RoundedRectangleVertex,Rectangle));
	glVertexAttribPointer(RoundedRectangleShader.aRoundingRadius,1,GL_FLOAT,GL_FALSE,sizeof(RoundedRectangleVertex),offsetof(RoundedRectangleVertex,RoundingRadius));
	glVertexAttribPointer(RoundedRectangleShader.aColor,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(RoundedRectangleVertex),offsetof(RoundedRectangleVertex,color));
	glVertexAttribPointer(RoundedRectangleShader.aIconColor,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(RoundedRectangleVertex),offsetof(RoundedRectangleVertex,IconColor));
}
LIST_IMPLEMENTATION(RoundedRectangleVertex)
void AppendRoundedRectangle(RoundedRectangleVertexList *verts, int x, int y, int z, int halfWidth, int halfHeight, float RoundingRadius, uint32_t color, uint32_t IconColor){
	RoundedRectangleVertex *v = RoundedRectangleVertexListMakeRoom(verts,6);
	int hwp = halfWidth+4;// pad by 4px to not cut off antialiased edges
	int hhp = halfHeight+4;

	v[0].Position[0] = -hwp + x;
	v[0].Position[1] = hhp + y;
	v[0].Position[2] = z;

	v[1].Position[0] = -hwp + x;
	v[1].Position[1] = -hhp + y;
	v[1].Position[2] = z;

	v[2].Position[0] = hwp + x;
	v[2].Position[1] = -hhp + y;
	v[2].Position[2] = z;

	v[3].Position[0] = v[2].Position[0];
	v[3].Position[1] = v[2].Position[1];
	v[3].Position[2] = v[2].Position[2];

	v[4].Position[0] = hwp + x;
	v[4].Position[1] = hhp + y;
	v[4].Position[2] = z;

	v[5].Position[0] = v[0].Position[0];
	v[5].Position[1] = v[0].Position[1];
	v[5].Position[2] = v[0].Position[2];

	for (int i = 0; i < 6; i++){
		v[i].Rectangle[0] = x;
		v[i].Rectangle[1] = y;
		v[i].Rectangle[2] = halfWidth;
		v[i].Rectangle[3] = halfHeight;
		v[i].RoundingRadius = RoundingRadius;
		v[i].color = color;
		v[i].IconColor = IconColor;
	}

	verts->used += 6;
}
typedef struct {
	float Position[3];
	float Texcoord[2];
} TextureVertex;
struct TextureShader {
	char *vertSrc;
	char *fragSrc;
	GLuint id;
	GLint aPosition;
	GLint aTexcoord;
	GLint uProj;
	GLint uTex;
} TextureShader = {
	"#version 110\n"
	"attribute vec3 aPosition;\n"
	"attribute vec2 aTexcoord;\n"
	"uniform mat4 uProj;\n"
	"varying vec2 Texcoord;\n"
	"void main(){\n"
	"	gl_Position = uProj * vec4(aPosition,1.0f);\n"
	"	Texcoord = aTexcoord;\n"
	"}",

	"#version 110\n"
	"uniform sampler2D uTex;\n"
	"varying vec2 Texcoord;\n"
	"void main(){\n"
	"	gl_FragColor = texture2D(uTex,Texcoord);\n"
	"}"
};
void CompileTextureShader(){
	TextureShader.id = CompileShader("TextureShader",TextureShader.vertSrc,TextureShader.fragSrc);
	TextureShader.aPosition = glGetAttribLocation(TextureShader.id,"aPosition");
	TextureShader.aTexcoord = glGetAttribLocation(TextureShader.id,"aTexcoord");
	TextureShader.uProj = glGetUniformLocation(TextureShader.id,"uProj");
	TextureShader.uTex = glGetUniformLocation(TextureShader.id,"uTex");
}
void TextureShaderPrepBuffer(){
	glEnableVertexAttribArray(TextureShader.aPosition);
	glEnableVertexAttribArray(TextureShader.aTexcoord);
	glVertexAttribPointer(TextureShader.aPosition,3,GL_FLOAT,GL_FALSE,sizeof(TextureVertex),0);
	glVertexAttribPointer(TextureShader.aTexcoord,2,GL_FLOAT,GL_FALSE,sizeof(TextureVertex),offsetof(TextureVertex,Texcoord));
}
void CompileShaders(){
	CompileRoundedRectangleShader();
	CompileTextureShader();
}
typedef struct {
	Texture texture;
	int charDims[95][2]; //width,height
	float uvs[95][4][2]; //corners
} CachedFont;
typedef struct {
	BITMAPINFOHEADER    bmiHeader;
	RGBQUAD             bmiColors[4];
} BITMAPINFO_TRUECOLOR32;
void GenCachedFont(CachedFont *cf, WCHAR *name, int height){
	HFONT hfont = CreateFontW(-height,0,0,0,FW_REGULAR,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_DONTCARE,name);

	// https://gamedev.net/forums/topic/617849-win32-draw-to-bitmap/4898762/
	HDC hdcScreen = GetDC(NULL);
	HDC hdcBmp = CreateCompatibleDC(hdcScreen);

	/*
	Chars will not have perfect packing, so we cannot trust total char area < total texture area as assurance that all chars will fit.
	Instead we have to iterate through the chars and increase our texture size if we run out of room, repeat until we no longer run out of room.
	*/
	HFONT oldFont = SelectObject(hdcBmp,hfont);
	SetBkMode(hdcBmp,TRANSPARENT);
	SetTextColor(hdcBmp,RGB(255,255,255));
	Image img;
	img.width = 64;
	img.height = 64;
	int x,y,gy;
	L0:
	x = 0; y = 0; gy = 0;
	for (WCHAR c = ' '; c <= '~'; c++){
		RECT r = {0};
		DrawTextW(hdcBmp,&c,1,&r,DT_CALCRECT|DT_NOPREFIX); // DT_NOPREFIX is required to prevent '&' from being treated as an underline prefix flag: https://stackoverflow.com/questions/20181949/how-to-stop-drawtext-from-underlining-alt-characters
		if ((y + r.bottom) > gy) gy = y+r.bottom;
		if ((x + r.right) >= img.width){
			y = gy;
			if ((y + r.bottom) >= img.height){
				img.width *= 2;
				img.height *= 2;
				goto L0;
			}
			gy = y+r.bottom;
			x = 0;
		}
		x += r.right;
	}

	BITMAPINFO_TRUECOLOR32 bmi = {0};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = img.width;
	bmi.bmiHeader.biHeight = img.height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB | BI_BITFIELDS;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiColors[0].rgbRed = 0xff;
	bmi.bmiColors[1].rgbGreen = 0xff;
	bmi.bmiColors[2].rgbBlue = 0xff;
	HBITMAP hbm = CreateDIBSection(hdcBmp,&bmi,DIB_RGB_COLORS,&img.pixels,0,0);
	HBITMAP hbmOld = SelectObject(hdcBmp,hbm);

	x = 0; y = 0; gy = 0;
	for (WCHAR c = ' '; c <= '~'; c++){
		RECT r = {0};
		DrawTextW(hdcBmp,&c,1,&r,DT_CALCRECT|DT_NOPREFIX);
		if ((y + r.bottom) > gy) gy = y+r.bottom;
		if ((x + r.right) >= img.width){
			y = gy;
			gy = y+r.bottom;
			x = 0;
		}
		ExtTextOutW(hdcBmp,x,y,0,0,&c,1,0);
		cf->charDims[c-' '][0] = r.right;
		cf->charDims[c-' '][1] = r.bottom;
		cf->uvs[c-' '][0][0] = (float) x / img.width;
		cf->uvs[c-' '][0][1] = (float) (img.height - 1 - y) / img.height;
		cf->uvs[c-' '][1][0] = cf->uvs[c-' '][0][0];
		cf->uvs[c-' '][1][1] = (float) (img.height - 1 - (y+r.bottom)) / img.height;
		cf->uvs[c-' '][2][0] = (float) (x + r.right) / img.width;
		cf->uvs[c-' '][2][1] = cf->uvs[c-' '][1][1];
		cf->uvs[c-' '][3][0] = cf->uvs[c-' '][2][0];
		cf->uvs[c-' '][3][1] = cf->uvs[c-' '][0][1];
		x += r.right;
	}

	SelectObject(hdcBmp,oldFont);
	DeleteObject(hfont);
	SelectObject(hdcBmp,hbmOld);
	DeleteDC(hdcBmp);
	ReleaseDC(NULL,hdcScreen);

	for (size_t i = 0; i < img.width*img.height; i++){
		uint8_t *p = img.pixels+i;
		uint8_t s;
		SWAP(s,p[0],p[2]);
		p[3] = max(p[0],max(p[1],p[2]));
	}
	TextureFromImage(&cf->texture,&img,false);
	DeleteObject(hbm);
}
int StringWidth(CachedFont *f, WCHAR *string, int charCount){
	int x = 0;
	for (int i = 0; i < charCount; i++){
		x += f->charDims[string[i]-' '][0];
	}
	return x;
}
LIST_IMPLEMENTATION(TextureVertex)
void AppendStringMesh(TextureVertexList *list, CachedFont *f, int x, int y, int z, WCHAR *s, int charCount){
	for (int i = 0; i < charCount; i++){
		TextureVertex *v = TextureVertexListMakeRoom(list,6);

		v[0].Position[0] = x;
		v[0].Position[1] = y + f->charDims[s[i]-' '][1];
		v[0].Position[2] = z;
		v[0].Texcoord[0] = f->uvs[s[i]-' '][0][0];
		v[0].Texcoord[1] = f->uvs[s[i]-' '][0][1];

		v[1].Position[0] = x;
		v[1].Position[1] = y;
		v[1].Position[2] = z;
		v[1].Texcoord[0] = f->uvs[s[i]-' '][1][0];
		v[1].Texcoord[1] = f->uvs[s[i]-' '][1][1];

		v[2].Position[0] = x + f->charDims[s[i]-' '][0];
		v[2].Position[1] = y;
		v[2].Position[2] = z;
		v[2].Texcoord[0] = f->uvs[s[i]-' '][2][0];
		v[2].Texcoord[1] = f->uvs[s[i]-' '][2][1];

		v[3] = v[2];

		v[4].Position[0] = v[2].Position[0];
		v[4].Position[1] = v[0].Position[1];
		v[4].Position[2] = z;
		v[4].Texcoord[0] = f->uvs[s[i]-' '][3][0];
		v[4].Texcoord[1] = f->uvs[s[i]-' '][3][1];

		v[5] = v[0];

		list->used += 6;
		x += f->charDims[s[i]-' '][0];
	}
}
void AppendCenteredStringMesh(TextureVertexList *list, CachedFont *f, int x, int y, int z, WCHAR *s, int charCount){
	AppendStringMesh(list,f,x-StringWidth(f,s,charCount)/2,y-f->charDims['l'-' '][1]/2,z,s,charCount);
}
void AppendFormatCenteredStringMesh(TextureVertexList *list, CachedFont *f, int x, int y, int z, WCHAR *format, ...){
	va_list args;
	va_start(args,format);
	static WCHAR str[512];
	int len = vswprintf(str,COUNT(str),format,args);
	AppendCenteredStringMesh(list,f,x,y,z,str,len);
	va_end(args);
}

int StringsAreEqual(const char* src, const char* dst, size_t dstlen){
	while (*src && dstlen-- && *dst)
	{
		if (*src++ != *dst++)
		{
			return 0;
		}
	}
	return (dstlen && *src == *dst) || (!dstlen && *src == 0);
}
HWND DarkGLMakeWindow(int iconId, WCHAR *title, int clientWidth, int clientHeight, WNDPROC windowProc){
	/*
	Modified from https://gist.github.com/mmozeiko/ed2ad27f75edf9c26053ce332a1f6647
	*/
	if (!wglChoosePixelFormatARB){
		// to get WGL functions we need valid GL context, so create dummy window for dummy GL contetx
		HWND dummy = CreateWindowExW(
			0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, NULL, NULL);
		Assert(dummy && "Failed to create dummy window");

		HDC dc = GetDC(dummy);
		Assert(dc && "Failed to get device context for dummy window");

		PIXELFORMATDESCRIPTOR desc =
		{
			.nSize = sizeof(desc),
			.nVersion = 1,
			.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			.iPixelType = PFD_TYPE_RGBA,
			.cColorBits = 24,
		};

		int format = ChoosePixelFormat(dc, &desc);
		if (!format)
		{
			FatalErrorA("Cannot choose OpenGL pixel format for dummy window!");
		}

		int ok = DescribePixelFormat(dc, format, sizeof(desc), &desc);
		Assert(ok && "Failed to describe OpenGL pixel format");

		// reason to create dummy window is that SetPixelFormat can be called only once for the window
		if (!SetPixelFormat(dc, format, &desc))
		{
			FatalErrorA("Cannot set OpenGL pixel format for dummy window!");
		}

		HGLRC rc = wglCreateContext(dc);
		Assert(rc && "Failed to create OpenGL context for dummy window");

		ok = wglMakeCurrent(dc, rc);
		Assert(ok && "Failed to make current OpenGL context for dummy window");

		// https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_extensions_string.txt
		PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB =
			(void*)wglGetProcAddress("wglGetExtensionsStringARB");
		if (!wglGetExtensionsStringARB)
		{
			FatalErrorA("OpenGL does not support WGL_ARB_extensions_string extension!");
		}

		const char* ext = wglGetExtensionsStringARB(dc);
		Assert(ext && "Failed to get OpenGL WGL extension string");

		const char* start = ext;
		for (;;)
		{
			while (*ext != 0 && *ext != ' ')
			{
				ext++;
			}

			size_t length = ext - start;
			if (StringsAreEqual("WGL_ARB_pixel_format", start, length))
			{
				// https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt
				wglChoosePixelFormatARB = (void*)wglGetProcAddress("wglChoosePixelFormatARB");
			}
			else if (StringsAreEqual("WGL_ARB_create_context", start, length))
			{
				// https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
				wglCreateContextAttribsARB = (void*)wglGetProcAddress("wglCreateContextAttribsARB");
			}
			else if (StringsAreEqual("WGL_EXT_swap_control", start, length))
			{
				// https://www.khronos.org/registry/OpenGL/extensions/EXT/WGL_EXT_swap_control.txt
				wglSwapIntervalEXT = (void*)wglGetProcAddress("wglSwapIntervalEXT");
			}

			if (*ext == 0)
			{
				break;
			}

			ext++;
			start = ext;
		}

		if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB || !wglSwapIntervalEXT)
		{
			FatalErrorA("OpenGL does not support required WGL extensions for modern context!");
		}

		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(rc);
		ReleaseDC(dummy, dc);
		DestroyWindow(dummy);
	}
	// register window class to have custom WindowProc callback
	WNDCLASSEXW wc =
	{
		.style = CS_HREDRAW | CS_VREDRAW,
		.cbSize = sizeof(wc),
		.lpfnWndProc = windowProc,
		.hInstance = GetModuleHandleW(0),
		.hIcon = LoadIconW(GetModuleHandleW(0),MAKEINTRESOURCEW(iconId)),
		.lpszClassName = L"opengl_window_class",
	};
	ATOM atom = RegisterClassExW(&wc);
	Assert(atom && "Failed to register window class");

	RECT wr = {0,0,clientWidth,clientHeight};
	AdjustWindowRect(&wr,WS_OVERLAPPEDWINDOW,FALSE);
	int wndWidth = wr.right-wr.left;
	int wndHeight = wr.bottom-wr.top;
	HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,title,WS_OVERLAPPEDWINDOW,GetSystemMetrics(SM_CXSCREEN)/2-wndWidth/2,GetSystemMetrics(SM_CYSCREEN)/2-wndHeight/2,wndWidth,wndHeight,0,0,wc.hInstance,0);
	Assert(hwnd && "Failed to create window");

	HDC dc = GetDC(hwnd);
	Assert(dc && "Failed to window device context");

	// set pixel format for OpenGL context
	{
		int attrib[] =
		{
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,     24,
			WGL_DEPTH_BITS_ARB,     24,
			WGL_STENCIL_BITS_ARB,   8,

			// uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
			// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
			//WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,

			// uncomment for multisampled framebuffer, from WGL_ARB_multisample extension
			// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
			//WGL_SAMPLE_BUFFERS_ARB, 1,
			//WGL_SAMPLES_ARB,        4, // 4x MSAA

			0,
		};

		int format;
		UINT formats;
		if (!wglChoosePixelFormatARB(dc, attrib, NULL, 1, &format, &formats) || formats == 0)
		{
			FatalErrorA("OpenGL does not support required pixel format!");
		}

		PIXELFORMATDESCRIPTOR desc = { .nSize = sizeof(desc) };
		int ok = DescribePixelFormat(dc, format, sizeof(desc), &desc);
		Assert(ok && "Failed to describe OpenGL pixel format");

		if (!SetPixelFormat(dc, format, &desc))
		{
			FatalErrorA("Cannot set OpenGL selected pixel format!");
		}
	}

	// create modern OpenGL context
	{
		int attrib[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
			WGL_CONTEXT_MINOR_VERSION_ARB, 0,
			0,
		};

		HGLRC rc = wglCreateContextAttribsARB(dc, NULL, attrib);
		if (!rc)
		{
			FatalErrorA("Cannot create modern OpenGL context! OpenGL version 4.5 not supported?");
		}

		BOOL ok = wglMakeCurrent(dc, rc);
		Assert(ok && "Failed to make current OpenGL context");

		// load OpenGL functions
#define X(type, name) name = (type)wglGetProcAddress(#name); Assert(name);
		GL_FUNCTIONS(X)
#undef X
	}

	wglSwapIntervalEXT(1);

	DWORD darkTitlebar = 1;
	int DwmwaUseImmersiveDarkMode = 20,
		DwmwaUseImmersiveDarkModeBefore20h1 = 19;
	SUCCEEDED(DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, &darkTitlebar, sizeof(darkTitlebar))) ||
		SUCCEEDED(DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkModeBefore20h1, &darkTitlebar, sizeof(darkTitlebar)));

	CompileShaders();

	srand(time(0));

	return hwnd;
}

/****************** Image Processing */
void GreyScale(Image *img){
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
	float d = 3.0f / (strength-1);
	float disx = 0.0f;
	float inv2pi = 1.0f / sqrtf(2.0f*M_PI);
	for (int i = 0; i < strength; i++){
		kernel[i] = expf(-0.5f*disx*disx)*inv2pi;
		disx += d;
	}
	float sum = 0.0f;
	for (int i = 0; i < strength; i++){
		sum += kernel[i];
	}
	sum = 1.0f / sum;
	for (int i = 0; i < strength; i++){
		kernel[i] *= sum;
	}
	Image b;
	b.width = img->width;
	b.height = img->height;
	b.pixels = MallocOrDie(b.width*b.height*sizeof(*b.pixels));
	float inv255 = 1.0f / 255.0f;
	for (int y = 0; y < img->height; y++){
		for (int x = 0; x < img->width; x++){
			float sums[3] = {0,0,0};
			for (int dx = -strength+1; dx < strength-1; dx++){
				uint8_t *p = img->pixels+y*img->width+CLAMP(x+dx,0,img->width-1);
				for (int i = 0; i < 3; i++){
					sums[i] = min(1.0f,sums[i]+inv255*p[i]*kernel[abs(dx)]);
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
					sums[i] = min(1.0f,sums[i]+inv255*p[i]*kernel[abs(dy)]);
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
}
void GreyScaleQuantize(Image *img, int divisions){
	int minGrey = 255;
	int maxGrey = 0;
	for (int i = 0; i < img->width*img->height; i++){
		uint8_t *p = img->pixels+i;
		int grey = min(255,0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]);
		if (grey < minGrey) minGrey = grey;
		else if (grey > maxGrey) maxGrey = grey;
		p[0] = grey;
		p[1] = grey;
		p[2] = grey;
	}

	int *invals = MallocOrDie((divisions)*sizeof(*invals));
	int *outvals = MallocOrDie((divisions)*sizeof(*invals));
	int id = (maxGrey-minGrey)/divisions;
	int od = 255/divisions;
	int iv = minGrey;
	int ov = 0;
	for (int i = 0; i < divisions; i++){
		invals[i] = iv;
		outvals[i] = ov;
		iv += id;
		ov += od;
	}
	invals[divisions-1] = maxGrey;
	outvals[divisions-1] = 255;

	for (int i = 0; i < img->width*img->height; i++){
		uint8_t *p = img->pixels+i;
		for (int j = 1; j < divisions; j++){
			if (p[0] <= invals[j]){
				if (p[0]-invals[j-1] > invals[j]-p[0]){
					p[0] = outvals[j];
					p[1] = outvals[j];
					p[2] = outvals[j];
					break;
				} else {
					p[0] = outvals[j-1];
					p[1] = outvals[j-1];
					p[2] = outvals[j-1];
					break;
				}
			}
		}
	}

	free(invals);
	free(outvals);
}
typedef struct {
	RECT rect;
	uint32_t color;
} ColorRect;
LIST_IMPLEMENTATION(ColorRect)
void RectangleDecompose(ColorRectList *list, Image *img, int minDim, int slop){
	LIST_FREE(list);
	//well we need some way to mark taken rectangles
	//alpha at the moment, idk. For jpg it's clearly getting filled in with 255s
	//we're just gonna set alpha to zero for taken rectangles
	for (int y = 0; y < img->height; y++){
		for (int x = 0; x < img->width; x++){
			uint32_t c = img->pixels[y*img->width+x];
			if (ALPHA(c)){
				bool vertical = randint(2);
				int i, j;
				int lim;
				if (vertical){
					lim = img->height;
					for (i = x; i < img->width; i++){
						for (j = y; j < lim; j++){
							if (img->pixels[j*img->width+i] != c){
								if (lim != img->height && abs(j-lim) > slop){
									j = lim;
									goto L0;
								} else {
									lim = j;
								}
							}
						}
					}
				} else {
					lim = img->width;
					for (j = y; j < img->height; j++){
						for (i = x; i < lim; i++){
							if (img->pixels[j*img->width+i] != c){
								if (lim != img->width && abs(i-lim) > slop){
									i = lim;
									goto L0;
								} else {
									lim = i;
								}
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
		for (int y = r->rect.bottom; y < r->rect.top; y++){
			for (int x = r->rect.left; x < r->rect.right; x++){
				img->pixels[y*img->width+x] = r->color;
			}
		}
	}
}

/********************** DarkWolken */
WCHAR gpath[MAX_PATH+16];
HWND gwnd;
HDC hdc;
int clientWidth = 800, clientHeight = 600;
float ortho[16];
CachedFont font;

typedef struct {
	GLuint vertexBuffer;
	int vertexCount;
} CachedMesh;
CachedMesh imageQuad;
typedef struct {
	Image image;
	Texture texture;
} ImageTexture;
ImageTexture images[5];
bool greyscale = false;
int gaussianBlurStrength = 9;
int quantizeDivisions = 4;
int rectangleDecomposeSeed;
int rectangleDecomposeMinDim = 25;
int rectangleDecomposeSlop = 0;

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
char *text;
size_t textLen;

void Update(){
	size_t size = images[0].image.width*images[0].image.height*sizeof(*images[0].image.pixels);
	memcpy(images[1].image.pixels,images[0].image.pixels,size);
	if (greyscale){
		GreyScale(&images[1].image);
	}
	GaussianBlur(&images[1].image,gaussianBlurStrength);
	memcpy(images[2].image.pixels,images[1].image.pixels,size);
	Quantize(&images[2].image,quantizeDivisions);
	memcpy(images[3].image.pixels,images[2].image.pixels,size);

	srand(rectangleDecomposeSeed);
	ColorRectList crl = {0};
	RectangleDecompose(&crl,&images[3].image,rectangleDecomposeMinDim,rectangleDecomposeSlop);

	//rectangle decompose, for each rectangle randomly choose whether to first expand along x or y. Expand along that dimension until you hit something, then expand along the other.
	//this way you can get random results each time
	//also pick the word for each rectangle with some randomness if constraints allow it.
	//allow for some configurable amount of slop for rectangles.

	for (ImageTexture *it = images; it < images+COUNT(images); it++){
		TextureFromImage(&it->texture,&it->image,false);
	}
}

typedef struct {
	int x, y, halfWidth, halfHeight, roundingRadius;
	uint32_t color, IconColor;
	WCHAR *string;
	void (*func)(void);
} Button;
void OpenImage(){
	IFileDialog *pfd;
	IShellItem *psi;
	PWSTR path = 0;
	COMDLG_FILTERSPEC fs = {L"Image files", L"*.png;*.jpg;*.jpeg"};
	if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog,0,CLSCTX_INPROC_SERVER,&IID_IFileOpenDialog,&pfd))){
		pfd->lpVtbl->SetFileTypes(pfd,1,&fs); 
		pfd->lpVtbl->SetTitle(pfd,L"Open Image");
		pfd->lpVtbl->Show(pfd,gwnd);
		if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd,&psi))){
			if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi,SIGDN_FILESYSPATH,&path))){
				for (ImageTexture *it = images; it < images+COUNT(images); it++){
					free(it->image.pixels);
				}
				LoadImageFromFile(&images[0].image,path,true);
				for (int i = 1; i < COUNT(images); i++){
					images[i].image.width = images[0].image.width;
					images[i].image.height = images[0].image.height;
					images[i].image.pixels = MallocOrDie(images[0].image.width*images[0].image.height*sizeof(*images[0].image.pixels));
				}
				Update();
				wcscpy(imagePath,path);
				imagePathLen = wcslen(imagePath);
				InvalidateRect(gwnd,0,0);
				CoTaskMemFree(path);
			}
			psi->lpVtbl->Release(psi);
		}
		pfd->lpVtbl->Release(pfd);
	}
}
LINKED_HASHLIST_IMPLEMENTATION(int)
typedef struct {
	int keylen;
	char *key;
	int value;
} IntBucket;
int CompareIntBuckets(IntBucket *a, IntBucket *b){
	return b->value - a->value;
}
void OpenText(){
	IFileDialog *pfd;
	IShellItem *psi;
	PWSTR path = 0;
	COMDLG_FILTERSPEC fs = {L"Text files", L"*.txt;"};
	if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog,0,CLSCTX_INPROC_SERVER,&IID_IFileOpenDialog,&pfd))){
		pfd->lpVtbl->SetFileTypes(pfd,1,&fs); 
		pfd->lpVtbl->SetTitle(pfd,L"Open Text");
		pfd->lpVtbl->Show(pfd,gwnd);
		if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd,&psi))){
			if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi,SIGDN_FILESYSPATH,&path))){
				if (text) free(text);
				text = LoadFileW(path,&textLen);

				//parse text
				intLinkedHashList hl = {0};
				char *prev = text;
				char *cur = text;
				char *end = text+textLen;
				while (1){
					while (cur != end){
						if (IsCharAlphaNumericA(*cur)) break;
						cur++;
					}
					if (cur == end) break;
					prev = cur;
					while (cur != end && IsCharAlphaNumericA(*cur)) cur++;
					int len = cur-prev;
					int *i = intLinkedHashListGet(&hl,len,prev);
					if (!i){
						i = intLinkedHashListNew(&hl,len,prev);
						*i = 1;
					} else {
						(*i)++;
					}
				}
				IntBucket *ib = MallocOrDie(hl.used*sizeof(*ib));
				int i = 0;
				for (intLinkedHashListBucket *b = hl.first; b; b = b->next){
					ib[i].keylen = b->keylen;
					ib[i].key = b->key;
					ib[i].value = b->value;
					i++;
				}
				qsort(ib,hl.used,sizeof(*ib),CompareIntBuckets);
				for (i = 0; i < hl.used; i++){
					printf("%.*s: %d\n",ib[i].keylen,ib[i].key,ib[i].value);
				}
				free(ib);
				LINKED_HASHLIST_FREE(&hl);

				wcscpy(textPath,path);
				textPathLen = wcslen(textPath);
				InvalidateRect(gwnd,0,0);
				CoTaskMemFree(path);
			}
			psi->lpVtbl->Release(psi);
		}
		pfd->lpVtbl->Release(pfd);
	}
}
void ToggleGreyscale();
void IncrementGaussianBlurStrength(){
	gaussianBlurStrength = min(50,gaussianBlurStrength+1);
	Update();
	InvalidateRect(gwnd,0,0);
}
void DecrementGaussianBlurStrength(){
	gaussianBlurStrength = max(0,gaussianBlurStrength-1);
	Update();
	InvalidateRect(gwnd,0,0);
}
void IncrementQuantizeDivisions(){
	quantizeDivisions = min(50,quantizeDivisions+1);
	Update();
	InvalidateRect(gwnd,0,0);
}
void DecrementQuantizeDivisions(){
	quantizeDivisions = max(0,quantizeDivisions-1);
	Update();
	InvalidateRect(gwnd,0,0);
}
void RandomizeRectSeed(){
	rectangleDecomposeSeed = rand();
	Update();
	InvalidateRect(gwnd,0,0);
}
void IncrementMinDim(){
	rectangleDecomposeMinDim = min(2000,rectangleDecomposeMinDim+5);
	Update();
	InvalidateRect(gwnd,0,0);
}
void DecrementMinDim(){
	rectangleDecomposeMinDim = max(0,rectangleDecomposeMinDim-5);
	Update();
	InvalidateRect(gwnd,0,0);
}
void IncrementSlop(){
	rectangleDecomposeSlop = min(100,rectangleDecomposeSlop+1);
	Update();
	InvalidateRect(gwnd,0,0);
}
void DecrementSlop(){
	rectangleDecomposeSlop = max(0,rectangleDecomposeSlop-1);
	Update();
	InvalidateRect(gwnd,0,0);
}
Button buttons[] = {
	{50,-14,46,10,10,0x7B9944 | (RR_DISH<<24),RGBA(0,0,0,RR_ICON_NONE),L"Open Image",OpenImage},
	{50,-14-26*1,46,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"Open Text",OpenText},
	{50,-14-26*2,46,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"Greyscale: Off",ToggleGreyscale},
	{14,-14-26*3,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"-",DecrementGaussianBlurStrength},{200,-14-26*3,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"+",IncrementGaussianBlurStrength},
	{14,-14-26*4,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"-",DecrementQuantizeDivisions},{200,-14-26*4,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"+",IncrementQuantizeDivisions},
	{50+68-46,-14-26*5,68,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"Randomize Rectangles",RandomizeRectSeed},
	{14,-14-26*6,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"-",DecrementMinDim},{200,-14-26*6,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"+",IncrementMinDim},
	{14,-14-26*7,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"-",DecrementSlop},{200,-14-26*7,10,10,10,RGBA(127,127,127,RR_DISH),RGBA(0,0,0,RR_ICON_NONE),L"+",IncrementSlop},
};
void ToggleGreyscale(){
	greyscale = !greyscale;
	buttons[2].string = greyscale ? L"Greyscale: On" : L"Greyscale: Off";
	Update();
	InvalidateRect(gwnd,0,0);
}
bool PointInButton(int buttonX, int buttonY, int halfWidth, int halfHeight, int x, int y){
	return abs(x-buttonX) < halfWidth && abs(y-buttonY) < halfHeight;
}

LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	switch (uMsg){
	case WM_CREATE:{
		DragAcceptFiles(hwnd,1);
		gwnd = hwnd;
		hdc = GetDC(hwnd);
		RECT wr;
		GetWindowRect(hwnd,&wr);
		SetWindowPos(hwnd,0,wr.left,wr.top,wr.right-wr.left,wr.bottom-wr.top,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);//this is a hack to prevent the single white frame when the window first appears
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:{
		clientWidth = LOWORD(lParam);
		clientHeight = HIWORD(lParam);
		glViewport(0,0,clientWidth,clientHeight);
		Float4x4Orthogonal(ortho,0,clientWidth,0,clientHeight,-10,10);
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
		for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
			if (PointInButton(b->x,clientHeight-1+b->y,b->halfWidth,b->halfHeight,x,y)){
				SetCursor(cursorFinger);
				return 0;
			}
		}
		SetCursor(scale == 1 ? cursorArrow : cursorPan);
		return 0;
	}
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:{
		int x = GET_X_LPARAM(lParam);
		int y = clientHeight - GET_Y_LPARAM(lParam) - 1;
		for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
			if (PointInButton(b->x,clientHeight-1+b->y,b->halfWidth,b->halfHeight,x,y)){
				b->func();
				return 0;
			}
		}
		/*
		if (PointInButton(20,workspaceHeight/2,10,25,x,y)){
			if (imageIndex > 0){
				imageIndex--;
				_snwprintf(gpath,COUNT(gpath),L"%s%s",currentFolder,images.elements[imageIndex]);
				TextureFromFile(&texture,gpath,interpolation);
				SetWindowTextW(gwnd,gpath);
				InvalidateRect(hwnd,0,0);
			}
		} else if (PointInButton(clientWidth-20,workspaceHeight/2,10,25,x,y)){
			if (imageIndex < images.used-1){
				imageIndex++;
				_snwprintf(gpath,COUNT(gpath),L"%s%s",currentFolder,images.elements[imageIndex]);
				TextureFromFile(&texture,gpath,interpolation);
				SetWindowTextW(gwnd,gpath);
				InvalidateRect(hwnd,0,0);
			}
		} else if (PointInButton(4+50,clientHeight-16,50,10,x,y)){
			interpolation = !interpolation;
			glBindTexture(GL_TEXTURE_2D,texture.id);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,interpolation ? GL_LINEAR : GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,interpolation ? GL_LINEAR : GL_NEAREST);
			InvalidateRect(hwnd,0,0);
		} else if (y < workspaceHeight && scale > 1){
			panPoint.x = x;
			panPoint.y = y;
			memcpy(originalPos,pos,sizeof(pos));
			pan = true;
		}*/
		return 0;
	}
	case WM_DROPFILES:{
		WCHAR p[MAX_PATH];
		if (DragQueryFileW(wParam,0xFFFFFFFF,0,0) > 1) goto EXIT_DROPFILES;
		DragQueryFileW(wParam,0,p,COUNT(p));
		//LoadImg(p);
	EXIT_DROPFILES:
		DragFinish(wParam);
		InvalidateRect(hwnd,0,0);
		return 0;
	}
	case WM_LBUTTONUP:{
		pan = false;
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
			pt.x -= pos[0];
			pt.y -= pos[1];
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
		glCheckError();

		glClearColor(0.122f,0.122f,0.137f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (imagePathLen){
			float totalHeight = (float)images[0].image.height*COUNT(images);
			float width = min(clientWidth,images[0].image.width);
			float height = width * (totalHeight/(float)images[0].image.width);
			if (height > clientHeight){
				height = clientHeight;
				width = height * ((float)images[0].image.width/totalHeight);
			}
			width *= scale;
			height *= scale;
			if (scale == 1){
				pos[0] = (clientWidth-(int)width)/2;
				pos[1] = (clientHeight-(int)height)/2;
				pos[2] = -1;
			}
			
			glUseProgram(TextureShader.id);
			glUniform1i(TextureShader.uTex,0);
			glBindBuffer(GL_ARRAY_BUFFER,imageQuad.vertexBuffer);
			TextureShaderPrepBuffer();
			float mata[16],matb[16],matc[16];
			float individualHeight = height/COUNT(images);
			for (int i = 0; i < COUNT(images); i++){
				glBindTexture(GL_TEXTURE_2D,images[i].texture.id);
				Float4x4Scale(matb,width,individualHeight,1);
				Float4x4Translation(mata,pos[0],pos[1]+i*individualHeight,pos[2]);
				Float4x4Multiply(mata,matb,matc);
				Float4x4Multiply(ortho,matc,mata);
				glUniformMatrix4fv(TextureShader.uProj,1,GL_FALSE,mata);
				glDrawArrays(GL_TRIANGLES,0,imageQuad.vertexCount);
			}
		}

		glUseProgram(RoundedRectangleShader.id);
		glUniformMatrix4fv(RoundedRectangleShader.proj,1,GL_FALSE,ortho);
		RoundedRectangleVertexList rrv = {0};
		for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
			AppendRoundedRectangle(&rrv,b->x,clientHeight-1+b->y,0,b->halfWidth,b->halfHeight,b->roundingRadius,b->color,b->IconColor);
		}
		GLuint buf;
		glGenBuffers(1,&buf);
		glBindBuffer(GL_ARRAY_BUFFER,buf);
		glBufferData(GL_ARRAY_BUFFER,rrv.used*sizeof(*rrv.elements),rrv.elements,GL_STATIC_DRAW);
		RoundedRectangleShaderPrepBuffer();
		glDrawArrays(GL_TRIANGLES,0,rrv.used);
		LIST_FREE(&rrv);

		glUseProgram(TextureShader.id);
		glUniform1i(TextureShader.uTex,0);
		glBindTexture(GL_TEXTURE_2D,font.texture.id);
		TextureVertexList tvl = {0};
		for (Button *b = buttons; b < buttons+COUNT(buttons); b++){
			AppendCenteredStringMesh(&tvl,&font,b->x,clientHeight-1+b->y,1,b->string,wcslen(b->string));
		}
		AppendFormatCenteredStringMesh(&tvl,&font,buttons[3].x + (buttons[4].x-buttons[3].x)/2,clientHeight-1+buttons[3].y,0,L"Gaussian Blur Strength: %d",gaussianBlurStrength);
		AppendFormatCenteredStringMesh(&tvl,&font,buttons[5].x + (buttons[6].x-buttons[5].x)/2,clientHeight-1+buttons[5].y,0,L"Quantize Divisions: %d",quantizeDivisions);
		AppendFormatCenteredStringMesh(&tvl,&font,buttons[8].x + (buttons[9].x-buttons[8].x)/2,clientHeight-1+buttons[8].y,0,L"Rect Min Dim: %d",rectangleDecomposeMinDim);
		AppendFormatCenteredStringMesh(&tvl,&font,buttons[10].x + (buttons[11].x-buttons[10].x)/2,clientHeight-1+buttons[10].y,0,L"Rect Slop: %d",rectangleDecomposeSlop);
		if (imagePathLen){
			AppendStringMesh(&tvl,&font,buttons[0].x + buttons[0].halfWidth+4,clientHeight-1+buttons[0].y-font.charDims['l'-' '][1]/2,0,imagePath,imagePathLen);
		}
		if (textPathLen){
			AppendStringMesh(&tvl,&font,buttons[1].x + buttons[1].halfWidth+4,clientHeight-1+buttons[1].y-font.charDims['l'-' '][1]/2,0,textPath,textPathLen);
		}
		glUniformMatrix4fv(TextureShader.uProj,1,GL_FALSE,ortho);
		glBufferData(GL_ARRAY_BUFFER,tvl.used*sizeof(*tvl.elements),tvl.elements,GL_STATIC_DRAW);
		TextureShaderPrepBuffer();
		glDrawArrays(GL_TRIANGLES,0,tvl.used);
		LIST_FREE(&tvl);

		glDeleteBuffers(1,&buf);

		SwapBuffers(hdc);
		ValidateRect(hwnd,0);
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
	CoInitialize(0);

	DarkGLMakeWindow(RID_ICON,L"DarkWolken",clientWidth,clientHeight,WindowProc); //loads OpenGL functions

	GenCachedFont(&font,L"Segoe UI",12);

	rectangleDecomposeSeed = rand();

	int argc;
	WCHAR **argv = CommandLineToArgvW(GetCommandLineW(),&argc);
	if (argc==2){
		//LoadImg(argv[1]);
	}

	glGenBuffers(1,&imageQuad.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER,imageQuad.vertexBuffer);
	TextureVertex v[6] = {
		{{0,1,0},{0,1}},
		{{0,0,0},{0,0}},
		{{1,0,0},{1,0}},
		{{1,0,0},{1,0}},
		{{1,1,0},{1,1}},
		{{0,1,0},{0,1}}
	};
	glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
	imageQuad.vertexCount = 6;
	glCheckError();

	cursorArrow = LoadCursorA(0,IDC_ARROW);
	cursorFinger = LoadCursorA(0,IDC_HAND);
	cursorPan = LoadCursorA(0,IDC_SIZEALL);
	SetCursor(cursorArrow);

	ShowWindow(gwnd,SW_SHOWDEFAULT);
	MSG msg;
	while (GetMessageW(&msg,0,0,0)){
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	CoUninitialize();
}