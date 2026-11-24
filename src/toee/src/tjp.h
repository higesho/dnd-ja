/* The Temple of Elemental Evil (Steam, TPlite: temple.exe + temple.dll) Japanese patch */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- tjp.c ---- */
/* 試験用の仕組み（窓の扱い、撮影、入力の差し込み、ログ）は試験版（build.bat test、/DTJP_TEST）だけに入る。
   配布版では g_test は 0、tlog は何もしない */
#ifdef TJP_TEST
extern int g_test;                      /* TJP_TEST=1 */
void tlog(const char *fmt, ...);
#else
#define g_test 0
#define tlog(...) ((void)0)
#endif
extern HMODULE g_self;
void patch_bytes(uint32_t at, const void *b, int n);
void patch_jmp(uint32_t at, void *to);
void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl);
const void *res_get(int id, DWORD *size);

/* ---- testio.c（試験版だけ） ---- */
#ifdef TJP_TEST
void testio_init(void); void testio_game_dll(HMODULE g);
void testio_present(void *swapchain);   /* IDXGISwapChain* */
#endif
