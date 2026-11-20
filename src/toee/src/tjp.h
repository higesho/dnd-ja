/* The Temple of Elemental Evil (Steam, TPlite: temple.exe + temple.dll) Japanese patch */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- tjp.c ---- */
extern int g_test;                      /* TJP_TEST=1 */
extern HMODULE g_self;
void tlog(const char *fmt, ...);
void patch_bytes(uint32_t at, const void *b, int n);
void patch_jmp(uint32_t at, void *to);
void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl);
const void *res_get(int id, DWORD *size);

/* ---- testio.c ---- */
void testio_init(void); void testio_game_dll(HMODULE g);
void testio_present(void *swapchain);   /* IDXGISwapChain* */
