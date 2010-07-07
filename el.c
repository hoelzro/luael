/*
* Copyright (c) 2010 Rob Hoelz <rob@hoelz.ro>
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

#define tofilep(L, narg) ((FILE **)luaL_checkudata((L), (narg), LUA_FILEHANDLE))

static EditLine *el = NULL;
static int callbacks_ref = LUA_NOREF;
static int luael_init(lua_State *);

static char *_run_prompt_fn(EditLine *_el)
{
    lua_State *L;
    int status;
    const char *result;

    el_get(_el, EL_CLIENTDATA, &L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
    lua_getfield(L, -1, "prompt");
    status = lua_pcall(L, 0, 1, 0);
    /*# what to do on error? */
    result = lua_tostring(L, -1);
    /*# what to do on error? */
    lua_pop(L, 2);

    /*# should this string be in Lua memory? */
    return (char *) result;
}

static char *_run_rprompt_fn(EditLine *_el)
{
    lua_State *L;
    int status;
    const char *result;

    el_get(_el, EL_CLIENTDATA, &L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
    lua_getfield(L, -1, "rprompt");
    status = lua_pcall(L, 0, 1, 0);
    /*# what to do on error? */
    result = lua_tostring(L, -1);
    /*# what to do on error? */
    lua_pop(L, 2);

    /*# should this string be in Lua memory? */
    return (char *) result;
}

static void check_el_inited(lua_State *L)
{
    if(! el) {
	lua_pushcfunction(L, luael_init);
	lua_call(L, 0, 0);
    }
}

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
    el_set(el, EL_CLIENTDATA, L);
    luaL_getmetatable(L, LUA_EDITLINE);
    lua_setmetatable(L, -2);
    luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L);
    callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
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

#define cond_init() if(0) { }
#define is(value) else if(! strcmp(param, value))

/*# error handling */
/* prompt/rprompt with extra argument as synonym for promptesc/rpromptesc? */
static int luael_set(lua_State *L)
{
    const char *param = luaL_checkstring(L, 1);

    check_el_inited(L);
    cond_init()
    is("prompt") {
        if(lua_isnoneornil(L, 2)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushnil(L);
            lua_setfield(L, -2, "prompt");
            el_set(el, EL_PROMPT, NULL);
        } else {
            luaL_checktype(L, 2, LUA_TFUNCTION);
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushvalue(L, 2);
            lua_setfield(L, -2, "prompt");
            el_set(el, EL_PROMPT, _run_prompt_fn);
        }
    }
    is("promptesc") {
        if(lua_isnoneornil(L, 2)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushnil(L);
            lua_setfield(L, -2, "prompt");
            el_set(el, EL_PROMPT_ESC, NULL);
        } else {
            const char *c;

            luaL_checktype(L, 2, LUA_TFUNCTION);
            c = luaL_checkstring(L, 3);
            if(strlen(c) != 1) {
                return luaL_error(L, "string '%s' is longer than one character", c);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushvalue(L, 2);
            lua_setfield(L, -2, "prompt");
            el_set(el, EL_PROMPT_ESC, _run_prompt_fn, *c);
        }
    }
    is("refresh") {
        el_set(el, EL_REFRESH);
    }
    is("rprompt") {
        if(lua_isnoneornil(L, 2)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushnil(L);
            lua_setfield(L, -2, "rprompt");
            el_set(el, EL_RPROMPT, NULL);
        } else {
            luaL_checktype(L, 2, LUA_TFUNCTION);
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushvalue(L, 2);
            lua_setfield(L, -2, "rprompt");
            el_set(el, EL_RPROMPT, _run_rprompt_fn);
        }
    }
    is("rpromptesc") {
        if(lua_isnoneornil(L, 2)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushnil(L);
            lua_setfield(L, -2, "rprompt");
            el_set(el, EL_RPROMPT_ESC, NULL);
        } else {
            const char *c;

            luaL_checktype(L, 2, LUA_TFUNCTION);
            c = luaL_checkstring(L, 3);
            if(strlen(c) != 1) {
                return luaL_error(L, "string '%s' is longer than one character", c);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
            lua_pushvalue(L, 2);
            lua_setfield(L, -2, "rprompt");
            el_set(el, EL_RPROMPT_ESC, _run_rprompt_fn, *c);
        }
    }
    is("terminal") {
        const char *term = luaL_checkstring(L, 2);
        el_set(el, EL_TERMINAL, term);
    }
    is("editor") {
        const char *editor = luaL_checkstring(L, 2);
        el_set(el, EL_EDITOR, editor);
    }
    is("signal") {
        int signal;
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        signal = lua_toboolean(L, 2);
        el_set(el, EL_SIGNAL, signal);
    }
    is("bind") {
        /* use el_parse */
    }
    is("echotc") {
        /* use el_parse */
    }
    is("settc") {
        /* use el_parse */
    }
    is("setty") {
        /* use el_parse */
    }
    is("telltc") {
        /* use el_parse */
    }
    is("addfn") {
        const char *name, *help;

        name = luaL_checkstring(L, 2);
        help = luaL_checkstring(L, 3);
        luaL_checktype(L, 4, LUA_TFUNCTION);
    }
    is("hist") {
        return luaL_error(L, "history functions not implemented yet!");
    }
    is("editmode") {
        int flag;

        luaL_checktype(L, 2, LUA_TBOOLEAN);
        flag = lua_toboolean(L, 2);
        el_set(el, EL_EDITMODE, flag);
    }
    is("getcfn") {
        luaL_checktype(L, 2, LUA_TFUNCTION);
    }
    is("fp") {
        int fd;
        FILE **fp;

        fd = luaL_checkinteger(L, 2);
        fp = tofilep(L, 3);
        if(*fp == NULL) {
            return luaL_error(L, "attempt to use a closed file");
        }
        el_set(el, EL_SETFP, fd, *fp);
    }
    else {
        return luaL_error(L, "Invalid parameter '%s' provided to el.set", param);
    }

    return 0;
}

/*# error handling */
static int luael_get(lua_State *L)
{
    const char *param = luaL_checkstring(L, 1);

    check_el_inited(L);
    cond_init()
    is("prompt") {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
        lua_getfield(L, -1, "prompt");
        return 1;
    }
    is("rprompt") {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks_ref);
        lua_getfield(L, -1, "rprompt");
        return 1;
    }
    is("editor") {
        const char *editor = NULL;
        el_get(el, EL_EDITOR, &editor);
        lua_pushstring(L, editor);
        return 1;
    }
    is("gettc") {
    }
    is("signal") {
    }
    is("editmode") {
    }
    is("getcfn") {
    }
    is("unbuffered") {
    }
    is("prepterm") {
    }
    is("fp") {
    }
    return 0;
}
#undef cond_init
#undef is

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
    { "set", luael_set },
    { "get", luael_get },
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
