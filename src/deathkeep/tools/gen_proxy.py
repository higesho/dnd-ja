# src/proxy.c と src/winmm.def を作る: システムの winmm.dll（32 ビット = SysWOW64）の全関数を中継する
import os, pefile
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
p = pefile.PE(r'C:\Windows\SysWOW64\winmm.dll')
names = [e.name.decode() for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name]
L = ['/* winmm.dll の代理: システムの winmm.dll（SysWOW64）の全関数を中継する（tools/gen_proxy.py で生成） */',
     '#include <windows.h>', '', '#define FUNCS(X) \\']
for i in range(0, len(names), 6):
    L.append('    ' + ' '.join('X(%s)' % n for n in names[i:i + 6]) + ' \\')
L[-1] = L[-1][:-2]
L += ['', '#define DECL(n) void *pf_##n;', 'FUNCS(DECL)', '', 'static HMODULE g_real;', '',
      'void proxy_init(void)', '{', '    char path[MAX_PATH];', '    UINT n = GetSystemDirectoryA(path, MAX_PATH);',
      '    lstrcpyA(path + n, "\\\\winmm.dll");', '    g_real = LoadLibraryA(path);', '    if (!g_real) return;',
      '#define LOAD(n) pf_##n = (void*)GetProcAddress(g_real, #n);', '    FUNCS(LOAD)', '}', '',
      '#define STUB(n) __declspec(naked) void p_##n(void) { __asm { jmp dword ptr [pf_##n] } }', 'FUNCS(STUB)', '']
open(os.path.join(ROOT, 'src', 'proxy.c'), 'w', encoding='utf-8').write('\n'.join(L))
open(os.path.join(ROOT, 'src', 'winmm.def'), 'w').write('LIBRARY winmm\nEXPORTS\n' + ''.join('    %s=p_%s\n' % (n, n) for n in names))
print(len(names), 'functions')
