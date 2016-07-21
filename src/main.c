// lr (lua run) Lua Interpreter with more flexible options
// Copyright (C) 2016 ushirotte <ushirotte at gmail dot com>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "panodef.h"

#define VERSION "1.0"
#define PROMPT "> "
#define lua_runstring(L,s) (luaL_loadstring(L, s) || lua_pcall(L,0,0,0))
#define lua_runfile(L,f) (luaL_loadfile(L, f) || lua_pcall(L,0,0,0))

static dword DEBUGMODE;

static const struct luaL_Reg lualibs_nodebug[] =
{
    { "",                 luaopen_base },
    { LUA_LOADLIBNAME,    luaopen_package },
    { LUA_TABLIBNAME,     luaopen_table },
    { LUA_IOLIBNAME,      luaopen_io },
    { LUA_OSLIBNAME,      luaopen_os },
    { LUA_STRLIBNAME,     luaopen_string },
    { LUA_MATHLIBNAME,    luaopen_math },
    { NULL, NULL }
};

// LUALIB_API
void LUAI_FUNC lua_openlibs_nodebug(lua_State* L)
{
    const luaL_Reg *lib=lualibs_nodebug;
    for (;lib->func;++lib) {
        lua_pushcfunction(L,lib->func);
        lua_pushstring(L,lib->name);
        lua_call(L,1,0);
    }
}

// LUALIB_API
int LUAI_FUNC l_debugcall(lua_State* L)
{
    luaL_checktype(L,1,LUA_TFUNCTION);

    if (DEBUGMODE) {
        lua_pcall(L,lua_gettop(L)-1,LUA_MULTRET,0);
        return lua_gettop(L);
    }
    return 0;
}


static inline void lua_poperror(lua_State *L)
{
    // fputs("Failed: ", stderr);
    fputs(lua_tostring(L, -1), stderr);
    fputs("\n", stderr);
    lua_pop(L, 1);
    return;
}


int main(int argc, char **argv)
{
    dword    dflag, vflag, iflag, cflag, stdflag;
    int      error;
    char     *str, *dostring, *filename;
    lua_State *L = luaL_newstate();
    // lua_openlibs_nodebug(L);
    luaL_openlibs(L);
    
    // _G.dbg( func, ... )
    lua_pushcfunction(L,l_debugcall);
    lua_pushvalue(L,-1);
    lua_setglobal(L,"dbgcall");
    lua_setglobal(L,"dbg");

    lua_newtable(L);    // file list
    lua_newtable(L);    // arg list

    dostring=NULL;
    dflag=false, vflag=false,
    iflag=false, cflag=false, stdflag=false;
    for (unsigned i=1, arg=0;i!=argc;++i) {
        if (arg) {                              // once arg is set, all params are considered arguments to be passed to lua via _G.arg
            lua_pushstring(L, argv[i]);
            lua_rawseti(L, -2, i-arg);
            continue;
        }

        if (*(str=argv[i]) != '-') {            // It's a file, add it to the list
            if (str[0]=='\0') {
                fputs("Empty string \'\' where filename was exepected\n", stderr);
                goto usage;
            }

            lua_pushstring(L, str);
            lua_rawseti(L, -3, lua_rawlen(L, -3)+1);
            continue;
        }

        if (isalnum(str[1]) && str[2]!='\0') {
            fputs("invalid option '", stderr);
            fputs(str, stderr);
            fputs("'\n", stderr);
            goto usage;
        }

        switch(str[1]) {
            case '\0':                                      // used to denote to stdin on file list
                if (stdflag) break;
                lua_pushstring(L, "");
                lua_rawseti(L, -3, lua_rawlen(L, -3)+1);
                stdflag=true;
                break;
            case 'd': dflag=DEBUGMODE=true; break;          // debug, todo: enable debug lib on flag set
            case 'v': vflag=true; break;                    // verbose
            case 'i': iflag=true; break;                    // interactive mode (line by line, with prompt)
            case 'c': cflag=true; break;                    // continue on error
            case 'h': goto usage;                           // display help
            case 'e':                                       // execute string
                if (!(argc-(i+1))) {
                    fputs("string expected for option '-e'\n", stderr);
                    lua_pop(L,2);
                    goto usage;
                }
                dostring=argv[++i];
                break;
            case '-':                                       // (--) GNU-style long params, set k,v pair on _G e.g --url=http://www.google.com
                {   char *div, *it, *st=str+2;

                    if (*st=='\0') { arg=i; continue; }     // --\0 - stop handling options/files, append the following args to _G.arg
                    if (!strcmp(st, "help")) goto usage;    // --help
                
                    if ((div=strchr(st, '='))) {
                        for (it=st;!isspace(*it) && it!=div;++it)
                            ;

                        if (st!=div && it==div) {
                            lua_pushglobaltable(L);
                            lua_pushlstring(L, st, div-st);

                            if (*(div+1)=='\0') {
                                fputs("expected value for key '", stderr);
                                fputs(lua_tostring(L, -1), stderr);
                                fputs("'\n", stderr);
                                goto usage;
                            }
                            
                            lua_pushstring(L, div+1);
                            lua_rawset(L, -3);
                            lua_pop(L,1);
                            break;
                        }
                    }
                }

            default:
                fputs("invalid option '", stderr);
                fputs(str, stderr);
                fputs("'\n", stderr);
                goto usage;
        }
    }
    
    // lua_setglobal(L, "arg");
    lua_pushglobaltable(L);
    lua_pushstring(L, "arg");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 2);

    if (dostring && lua_runstring(L,dostring)) {  // handle -e option
        lua_poperror(L);
        if (!cflag) goto fail;
    }

    for (unsigned i=0,sz=lua_rawlen(L,-1);i!=sz;++i) {
        lua_rawgeti(L, -1, i+1);
        filename=(char*)lua_tostring(L,-1);
        
        {   lua_pushglobaltable(L); // set _FILE
            lua_pushstring(L, "_FILE");
            
            if (stdflag && filename[0]=='\0') { // handle non-interactive standard input
                filename=NULL;
                lua_pushnil(L);
            } else {
                lua_pushvalue(L, -3);
            }
            
            lua_rawset(L, -3);
        }

        if (lua_runfile(L, filename)) { // todo: verbose option (use lua_load with reader when you do)
            lua_poperror(L);
            if (!cflag) goto fail;
        }

        {   lua_pushstring(L, "_FILE"); // unset _FILE
            lua_pushnil(L);
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop _G
        }

        lua_pop(L, 1); // pop filename
    }

    char buffer[256];
    if (iflag || (!dostring && !stdflag && !lua_rawlen(L,-1))) { // run interactive mode if -i is set or nothing has been done
        if (stdflag)
            clearerr(stdin); // was used by the - option, reset it
        
        fputs("lrun " VERSION " " LUA_VERSION " (", stderr);
        fputs(argv[0], stderr);
        fputs(")\n" PROMPT, stderr);

        while(fgets(buffer, sizeof buffer, stdin) != NULL) {
            if (lua_runstring(L, buffer))
                lua_poperror(L);

            fputs(PROMPT, stderr);
        }

        fputc('\n', stderr);
    }

    return (0);
usage:
    fputs("Usage: ", stderr);
    fputs(argv[0], stderr);
    fputs(  " [options] [scripts [-- args ]]\n"
            "\t-e 's'\tLoad and execute string 's'\n"
            "\t-i\tStart interactive mode after executing all scripts\n"
            "\t\tIf no scripts are specified, this mode will begin by default\n"
            // "\t-v\t\tVerbose. Print file contents as they're loaded."
            // todo: -l lib (load lib via require)
            "\t-d\tDebug mode; enable debug library and allow calls with _G.dbg() and _G.dbgcall()\n"
            "\t\te.g. dbgcall(print, \"foo\")\n"
            "\t-\tLoad and execute input from stdin\n"
            "\t--\tStop handling arguments. All subsequent arguments passed are appended to _G.arg\n"
            "\t--k=v\tDefine _G[ k ] as string 'v'\n"
            "\t\te.g. --url=http://www.google.com\n"
            "\t--help\n"
            "\t-h\tDisplay this menu\n"
            , stderr);
fail:
    lua_settop(L, 0);
    return (1);
}

