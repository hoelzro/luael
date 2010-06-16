/*
* Copyright (c) 2010 Rob Hoelz <rob@hoelzro.net>
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <histedit.h>
#include <stdio.h>
#include <string.h>

#define LUA_EDITLINE "EditLine*"
#define LIB_NAME "el"
#define LUA_PUSHERROR(L) lua_pushnil(L); lua_pushstring(L, strerror(errno))

static EditLine *el = NULL;

static int luael_init(lua_State *L)
{
    const char *prog = "lua";
    FILE *fin = stdin;
    FILE *fout = stdout;
    FILE *ferr = stderr;
    EditLine **elp;

    if(! lua_isnone(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);

        lua_getfield(L, 1, "prog");
        if(! lua_isnil(L, -1)) {
            prog = luaL_checkstring(L, -1);
        }
        lua_getfield(L, 1, "fin");
        if(! lua_isnil(L, -1)) {
            fin = *((FILE **) luaL_checkudata(L, -1, LUA_FILEHANDLE));
        }
        lua_getfield(L, 1, "fout");
        if(! lua_isnil(L, -1)) {
            fout = *((FILE **) luaL_checkudata(L, -1, LUA_FILEHANDLE));
        }
        lua_getfield(L, 1, "ferr");
        if(! lua_isnil(L, -1)) {
            ferr = *((FILE **) luaL_checkudata(L, -1, LUA_FILEHANDLE));
        }

        lua_pop(L, 4);
    }

    elp = lua_newuserdata(L, sizeof(EditLine *));
    el = *elp = el_init(prog, fin, fout, ferr);
    luaL_getmetatable(L, LUA_EDITLINE);
    lua_setmetatable(L, -2);
    luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static void check_el_inited(lua_State *L)
{
    if(! el) {
	lua_pushcfunction(L, luael_init);
	lua_call(L, 0, 0);
    }
}

static int luael_gets(lua_State *L)
{
    const char *line;
    int count;

    check_el_inited(L);
    line = el_gets(el, &count);
    if(line) {
	lua_pushlstring(L, line, count);
    } else {
	if(count == -1) {
	    LUA_PUSHERROR(L);
	    return 2;
	} else {
	    lua_pushliteral(L, "");
	}
    }
    return 1;
}

static int luael_getc(lua_State *L)
{
    int result;
    char ch;

    check_el_inited(L);
    result = el_getc(el, &ch);
    if(result == -1) {
	LUA_PUSHERROR(L);
	return 2;
    } else {
	lua_pushlstring(L, &ch, 1);
    }
    return 1;
}

static int luael_push(lua_State *L)
{
    const char *str;

    check_el_inited(L);
    str = luaL_checkstring(L, 1);
    el_push(el, str);
    return 0;
}

static int _el_gc(lua_State *L)
{
    EditLine **elp = (EditLine **) luaL_checkudata(L, -1, LUA_EDITLINE);
    el_end(*elp);
    return 0;
}

static luaL_Reg luael_functions[] = {
    { "init", luael_init },
    { "gets", luael_gets },
    { "getc", luael_getc },
    { "push", luael_push },
    { NULL, NULL }
};

int luaopen_el(lua_State *L)
{
    luaL_newmetatable(L, LUA_EDITLINE);
    lua_pushcfunction(L, _el_gc);
    lua_setfield(L, -2, "__gc");
    luaL_register(L, LIB_NAME, luael_functions);
    return 1;
}
