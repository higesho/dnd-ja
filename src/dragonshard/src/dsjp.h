/* Dungeons & Dragons: Dragonshard (Dragonshard.exe 2005, image base 0x400000, no relocs) Japanese patch */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- dsjp.c ---- */
extern int g_test;                      /* DSJP_TEST=1 */
extern HMODULE g_self;
void dlog(const char *fmt, ...);
void patch_jmp(uint32_t at, void *to);
void patch_call(uint32_t at, void *to);
void patch_bytes(uint32_t at, const void *b, int n);
uint32_t find_code(uint32_t fixed, const uint8_t *pat, const char *mask);   /* 0 = not found */
void *iat_hook(const char *dll, const char *func, void *repl);
const void *res_get(int id, DWORD *size);

/* ---- testio.c ---- */
void testio_init(void);
void testio_present(void *dev);         /* IDirect3DDevice9* */
HRESULT testio_wrap_dinput(void *di);   /* IDirectInput8A* */

/* ---- explode.c ---- */
int explode(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstlen);  /* -> bytes written or -1 */

/* ---- arch.c ---- */
void arch_init(void);
const WCHAR *arch_redirect(const WCHAR *path);   /* generated archive for Archives\Text.H2O etc, or NULL */

/* ---- font.c ---- */
int font_init(void);
int font_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn);

/* ---- wrap.c ---- */
void wrap_init(void);
void glyph_init(void);

/* ---- text.c ---- */
void text_init(void);
int text_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn);
const uint16_t *text_charset(int *n);  /* every non-ASCII char the translation uses */
