/* 自動生成: 本物の wsock32.dll への中継（gen は gen_wsock32.py 相当の処理） */
#define JA_FWD_COUNT 75
static const char *const ja_fwd_name[JA_FWD_COUNT] = {
    "accept",
    "bind",
    "closesocket",
    "connect",
    "getpeername",
    "getsockname",
    "getsockopt",
    "htonl",
    "htons",
    "inet_addr",
    "inet_ntoa",
    "ioctlsocket",
    "listen",
    "ntohl",
    "ntohs",
    "recv",
    "recvfrom",
    "select",
    "send",
    "sendto",
    "setsockopt",
    "shutdown",
    "socket",
    "MigrateWinsockConfiguration",
    "gethostbyaddr",
    "gethostbyname",
    "getprotobyname",
    "getprotobynumber",
    "getservbyname",
    "getservbyport",
    "gethostname",
    "WSAAsyncSelect",
    "WSAAsyncGetHostByAddr",
    "WSAAsyncGetHostByName",
    "WSAAsyncGetProtoByNumber",
    "WSAAsyncGetProtoByName",
    "WSAAsyncGetServByPort",
    "WSAAsyncGetServByName",
    "WSACancelAsyncRequest",
    "WSASetBlockingHook",
    "WSAUnhookBlockingHook",
    "WSAGetLastError",
    "WSASetLastError",
    "WSACancelBlockingCall",
    "WSAIsBlocking",
    "WSAStartup",
    "WSACleanup",
    "__WSAFDIsSet",
    "WEP",
    "WSApSetPostRoutine",
    "inet_network",
    "getnetbyname",
    "rcmd",
    "rexec",
    "rresvport",
    "sethostname",
    "dn_expand",
    "WSARecvEx",
    "s_perror",
    "GetAddressByNameA",
    "GetAddressByNameW",
    "EnumProtocolsA",
    "EnumProtocolsW",
    "GetTypeByNameA",
    "GetTypeByNameW",
    "GetNameByTypeA",
    "GetNameByTypeW",
    "SetServiceA",
    "SetServiceW",
    "GetServiceA",
    "GetServiceW",
    "NPLoadNameSpaces",
    "TransmitFile",
    "AcceptEx",
    "GetAcceptExSockaddrs",
};
static FARPROC ja_fwd_ptr[JA_FWD_COUNT];
__declspec(naked) void ja_fwd_0(void) { __asm { jmp dword ptr [ja_fwd_ptr + 0] } }
__declspec(naked) void ja_fwd_1(void) { __asm { jmp dword ptr [ja_fwd_ptr + 4] } }
__declspec(naked) void ja_fwd_2(void) { __asm { jmp dword ptr [ja_fwd_ptr + 8] } }
__declspec(naked) void ja_fwd_3(void) { __asm { jmp dword ptr [ja_fwd_ptr + 12] } }
__declspec(naked) void ja_fwd_4(void) { __asm { jmp dword ptr [ja_fwd_ptr + 16] } }
__declspec(naked) void ja_fwd_5(void) { __asm { jmp dword ptr [ja_fwd_ptr + 20] } }
__declspec(naked) void ja_fwd_6(void) { __asm { jmp dword ptr [ja_fwd_ptr + 24] } }
__declspec(naked) void ja_fwd_7(void) { __asm { jmp dword ptr [ja_fwd_ptr + 28] } }
__declspec(naked) void ja_fwd_8(void) { __asm { jmp dword ptr [ja_fwd_ptr + 32] } }
__declspec(naked) void ja_fwd_9(void) { __asm { jmp dword ptr [ja_fwd_ptr + 36] } }
__declspec(naked) void ja_fwd_10(void) { __asm { jmp dword ptr [ja_fwd_ptr + 40] } }
__declspec(naked) void ja_fwd_11(void) { __asm { jmp dword ptr [ja_fwd_ptr + 44] } }
__declspec(naked) void ja_fwd_12(void) { __asm { jmp dword ptr [ja_fwd_ptr + 48] } }
__declspec(naked) void ja_fwd_13(void) { __asm { jmp dword ptr [ja_fwd_ptr + 52] } }
__declspec(naked) void ja_fwd_14(void) { __asm { jmp dword ptr [ja_fwd_ptr + 56] } }
__declspec(naked) void ja_fwd_15(void) { __asm { jmp dword ptr [ja_fwd_ptr + 60] } }
__declspec(naked) void ja_fwd_16(void) { __asm { jmp dword ptr [ja_fwd_ptr + 64] } }
__declspec(naked) void ja_fwd_17(void) { __asm { jmp dword ptr [ja_fwd_ptr + 68] } }
__declspec(naked) void ja_fwd_18(void) { __asm { jmp dword ptr [ja_fwd_ptr + 72] } }
__declspec(naked) void ja_fwd_19(void) { __asm { jmp dword ptr [ja_fwd_ptr + 76] } }
__declspec(naked) void ja_fwd_20(void) { __asm { jmp dword ptr [ja_fwd_ptr + 80] } }
__declspec(naked) void ja_fwd_21(void) { __asm { jmp dword ptr [ja_fwd_ptr + 84] } }
__declspec(naked) void ja_fwd_22(void) { __asm { jmp dword ptr [ja_fwd_ptr + 88] } }
__declspec(naked) void ja_fwd_23(void) { __asm { jmp dword ptr [ja_fwd_ptr + 92] } }
__declspec(naked) void ja_fwd_24(void) { __asm { jmp dword ptr [ja_fwd_ptr + 96] } }
__declspec(naked) void ja_fwd_25(void) { __asm { jmp dword ptr [ja_fwd_ptr + 100] } }
__declspec(naked) void ja_fwd_26(void) { __asm { jmp dword ptr [ja_fwd_ptr + 104] } }
__declspec(naked) void ja_fwd_27(void) { __asm { jmp dword ptr [ja_fwd_ptr + 108] } }
__declspec(naked) void ja_fwd_28(void) { __asm { jmp dword ptr [ja_fwd_ptr + 112] } }
__declspec(naked) void ja_fwd_29(void) { __asm { jmp dword ptr [ja_fwd_ptr + 116] } }
__declspec(naked) void ja_fwd_30(void) { __asm { jmp dword ptr [ja_fwd_ptr + 120] } }
__declspec(naked) void ja_fwd_31(void) { __asm { jmp dword ptr [ja_fwd_ptr + 124] } }
__declspec(naked) void ja_fwd_32(void) { __asm { jmp dword ptr [ja_fwd_ptr + 128] } }
__declspec(naked) void ja_fwd_33(void) { __asm { jmp dword ptr [ja_fwd_ptr + 132] } }
__declspec(naked) void ja_fwd_34(void) { __asm { jmp dword ptr [ja_fwd_ptr + 136] } }
__declspec(naked) void ja_fwd_35(void) { __asm { jmp dword ptr [ja_fwd_ptr + 140] } }
__declspec(naked) void ja_fwd_36(void) { __asm { jmp dword ptr [ja_fwd_ptr + 144] } }
__declspec(naked) void ja_fwd_37(void) { __asm { jmp dword ptr [ja_fwd_ptr + 148] } }
__declspec(naked) void ja_fwd_38(void) { __asm { jmp dword ptr [ja_fwd_ptr + 152] } }
__declspec(naked) void ja_fwd_39(void) { __asm { jmp dword ptr [ja_fwd_ptr + 156] } }
__declspec(naked) void ja_fwd_40(void) { __asm { jmp dword ptr [ja_fwd_ptr + 160] } }
__declspec(naked) void ja_fwd_41(void) { __asm { jmp dword ptr [ja_fwd_ptr + 164] } }
__declspec(naked) void ja_fwd_42(void) { __asm { jmp dword ptr [ja_fwd_ptr + 168] } }
__declspec(naked) void ja_fwd_43(void) { __asm { jmp dword ptr [ja_fwd_ptr + 172] } }
__declspec(naked) void ja_fwd_44(void) { __asm { jmp dword ptr [ja_fwd_ptr + 176] } }
__declspec(naked) void ja_fwd_45(void) { __asm { jmp dword ptr [ja_fwd_ptr + 180] } }
__declspec(naked) void ja_fwd_46(void) { __asm { jmp dword ptr [ja_fwd_ptr + 184] } }
__declspec(naked) void ja_fwd_47(void) { __asm { jmp dword ptr [ja_fwd_ptr + 188] } }
__declspec(naked) void ja_fwd_48(void) { __asm { jmp dword ptr [ja_fwd_ptr + 192] } }
__declspec(naked) void ja_fwd_49(void) { __asm { jmp dword ptr [ja_fwd_ptr + 196] } }
__declspec(naked) void ja_fwd_50(void) { __asm { jmp dword ptr [ja_fwd_ptr + 200] } }
__declspec(naked) void ja_fwd_51(void) { __asm { jmp dword ptr [ja_fwd_ptr + 204] } }
__declspec(naked) void ja_fwd_52(void) { __asm { jmp dword ptr [ja_fwd_ptr + 208] } }
__declspec(naked) void ja_fwd_53(void) { __asm { jmp dword ptr [ja_fwd_ptr + 212] } }
__declspec(naked) void ja_fwd_54(void) { __asm { jmp dword ptr [ja_fwd_ptr + 216] } }
__declspec(naked) void ja_fwd_55(void) { __asm { jmp dword ptr [ja_fwd_ptr + 220] } }
__declspec(naked) void ja_fwd_56(void) { __asm { jmp dword ptr [ja_fwd_ptr + 224] } }
__declspec(naked) void ja_fwd_57(void) { __asm { jmp dword ptr [ja_fwd_ptr + 228] } }
__declspec(naked) void ja_fwd_58(void) { __asm { jmp dword ptr [ja_fwd_ptr + 232] } }
__declspec(naked) void ja_fwd_59(void) { __asm { jmp dword ptr [ja_fwd_ptr + 236] } }
__declspec(naked) void ja_fwd_60(void) { __asm { jmp dword ptr [ja_fwd_ptr + 240] } }
__declspec(naked) void ja_fwd_61(void) { __asm { jmp dword ptr [ja_fwd_ptr + 244] } }
__declspec(naked) void ja_fwd_62(void) { __asm { jmp dword ptr [ja_fwd_ptr + 248] } }
__declspec(naked) void ja_fwd_63(void) { __asm { jmp dword ptr [ja_fwd_ptr + 252] } }
__declspec(naked) void ja_fwd_64(void) { __asm { jmp dword ptr [ja_fwd_ptr + 256] } }
__declspec(naked) void ja_fwd_65(void) { __asm { jmp dword ptr [ja_fwd_ptr + 260] } }
__declspec(naked) void ja_fwd_66(void) { __asm { jmp dword ptr [ja_fwd_ptr + 264] } }
__declspec(naked) void ja_fwd_67(void) { __asm { jmp dword ptr [ja_fwd_ptr + 268] } }
__declspec(naked) void ja_fwd_68(void) { __asm { jmp dword ptr [ja_fwd_ptr + 272] } }
__declspec(naked) void ja_fwd_69(void) { __asm { jmp dword ptr [ja_fwd_ptr + 276] } }
__declspec(naked) void ja_fwd_70(void) { __asm { jmp dword ptr [ja_fwd_ptr + 280] } }
__declspec(naked) void ja_fwd_71(void) { __asm { jmp dword ptr [ja_fwd_ptr + 284] } }
__declspec(naked) void ja_fwd_72(void) { __asm { jmp dword ptr [ja_fwd_ptr + 288] } }
__declspec(naked) void ja_fwd_73(void) { __asm { jmp dword ptr [ja_fwd_ptr + 292] } }
__declspec(naked) void ja_fwd_74(void) { __asm { jmp dword ptr [ja_fwd_ptr + 296] } }
