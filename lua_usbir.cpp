#include <lua.hpp>
#include <usbir.h>
#include <string.h>

// Luaでのオブジェクト名
#define USBIR_METATABLE "USBIR.device"

// ヘルパー：LuaのuserdataをUSBIRDevice構造体に変換
static USBIRDevice** to_dev(lua_State *L) {
    return (USBIRDevice**)luaL_checkudata(L, 1, USBIR_METATABLE);
}

// Lua: usbir.open()
static int l_open(lua_State *L) {
    USBIRDevice *dev = openUSBIR();
    if (!dev) {
        lua_pushnil(L);
        lua_pushstring(L, "Could not open USB IR device.");
        return 2;
    }
    // Luaのメモリ管理下にポインタを格納
    USBIRDevice **ud = (USBIRDevice**)lua_newuserdata(L, sizeof(USBIRDevice*));
    *ud = dev;
    luaL_getmetatable(L, USBIR_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

// Lua: dev:send(format, data_string)
static int l_send(lua_State *L) {
    USBIRDevice *dev = *to_dev(L);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    if (len < 3) return luaL_error(L, "Invalid data format");
    int format_type = (int)(data[0]);
    int code_len1 = (int)(data[1]);
    int code_len2 = (int)(data[2]);

    // ライブラリの関数を呼び出し
    int res = writeUSBIRex(dev, format_type, (unsigned char*)(data) + 3, code_len1, code_len2);
    lua_pushboolean(L, res == 0);
    return 1;
}

// Lua: dev:receive()
static int l_receive(lua_State *L) {
    USBIRDevice *dev = *to_dev(L);
    unsigned char buffer[PKT_SIZE];
    
    int res = readUSBIRex(dev, buffer);
    if (res > 0) {
        lua_pushlstring(L, (const char*)buffer, 3 + ((int)(buffer[1]) + (int)(buffer[2])) / 8);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// 解放処理（LuaのGCが走った時や dev:close() で呼ばれる）
static int l_close(lua_State *L) {
    USBIRDevice **ud = to_dev(L);
    if (*ud) {
        closeUSBIR(*ud);
        *ud = NULL;
    }
    return 0;
}

// モジュールの関数テーブル
static const struct luaL_Reg usbir_funcs[] = {
    {"open", l_open},
    {NULL, NULL}
};

// メソッドテーブル（オブジェクトが持つ関数）
static const struct luaL_Reg usbir_methods[] = {
    {"send", l_send},
    {"receive", l_receive},
    {"close", l_close},
    {"__gc", l_close}, // 自動解放用
    {NULL, NULL}
};

extern "C" int luaopen_usbir(lua_State *L) {
    // メタテーブルの作成
    luaL_newmetatable(L, USBIR_METATABLE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, usbir_methods, 0);

    // モジュール自体の登録
    luaL_newlib(L, usbir_funcs);
    return 1;
}

