/* Forgotten Realms: Demon Stone (Steam, DemonStone.exe, image base 0x400000) Japanese patch */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- dst.c ---- */
/* 試験用の仕組み（窓のまま動かす、撮影、入力の差し込み、ログ）は試験版（build.bat test、/DDSTJP_TEST）だけに入る。
   配布版では g_test は 0、dlog は何もしない */
#ifdef DSTJP_TEST
extern int g_test;                      /* DSTJP_TEST=1 */
void dlog(const char *fmt, ...);
#else
#define g_test 0
#define dlog(...) ((void)0)
#endif
extern HMODULE g_self;
void patch_jmp(uint32_t at, void *to);
void patch_bytes(uint32_t at, const void *b, int n);
uint32_t find_code(uint32_t fixed, const uint8_t *pat, const char *mask);   /* 0 = not found */
void *iat_hook_mod(HMODULE mod, const char *dll, const char *func, void *repl);
const void *res_get(int id, DWORD *size);
const WCHAR *work_dir(void);            /* %TEMP%\DemonStoneJP\ */

/* ---- testio.c（試験版だけ） ---- */
#ifdef DSTJP_TEST
void testio_init(void);
void testio_present(void *dev);         /* IDirect3DDevice9* */
HRESULT testio_wrap_dinput(void *di);   /* IDirectInput8A* */
#endif

/* ---- data.c ---- */
int data_init(void);                    /* translation table + char set from the embedded blob */
const uint16_t *data_lookup(const uint16_t *en, int n, int *outn);   /* Japanese for an English string, or NULL */
const uint16_t *data_charset(int *n);   /* every non-ASCII char the translation uses */

/* ---- files.c ---- */
const char *files_redirect(const char *path);   /* generated file for UI\*.OBX, subtitle\*.OBX, or NULL */
uint8_t *files_read(const char *path, uint32_t *n);
int files_write(const WCHAR *path, const uint8_t *b, uint32_t n);
uint32_t files_stamp(void);             /* changes with the translation table and the build format */

/* ---- font.c ---- */
int font_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn);
void font_init(void);                   /* swap each XFT the game sets up for one with Japanese glyphs */

/* ---- wrap.c ---- */
void wrap_init(void);
