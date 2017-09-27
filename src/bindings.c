#include "defs.h"
#include "app.h"
#include "skunkpad.h"
#include "ui.h"
#include "str.h"
#include "res_load.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static const stringref c_script = str_lit("skunkpad.lua");

static lua_State* g_lua = 0;


static void message(const char *message, const char *title) {
	if (!title) title = "Lua Error";
	ui_report_error(title, message);
}


// ------------------------- from lbaselib.c -------------------------

static int print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i, num = 0;
  const char *s;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    if (i>1) { lua_pushstring(L, " "); num++; }
    lua_pushvalue(L, -(1+num)); /* re-push tostring function */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("print"));
	num++;
  }
  lua_concat(L, num);
  s = lua_tostring(L, -1);
  if (s) message(s, "Lua Print");
  return 0;
}


// ------------------------- from lua.c -------------------------

static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    message(msg, 0);
    lua_pop(L, 1);
  }
  return status;
}

static int traceback (lua_State *L) {
  if (!lua_isstring(L, 1))  /* 'message' not a string? */
    return 1;  /* keep it intact */
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

static int docall (lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  lua_remove(L, base);  /* remove traceback function */
  return status;
}


// --------------------- tagged objects ---------------------

static const char* c_frame_tag = "Frame";
static const char* c_image_tag = "Image";
static const char* c_timer_tag = "Timer";

static void* get_tagged(lua_State *L, int idx, const char* tag) // [-0, +0]
{
	void *check, *result = lua_touserdata(L, idx);
	if (result) {
		lua_pushvalue(L, idx);            // +1     push userdata
		lua_rawget(L, LUA_REGISTRYINDEX); // -1,+1  key on userdata
		check = lua_touserdata(L, -1);    //        type tag, nil or other
		lua_pop(L, 1);                    // -1     balance stack
		if (check == tag)                 //        verify type tag
			return result;
	}
	luaL_error(L, "expecting %s for argument %d", tag, idx);
	return 0;
}

static void push_tagged(lua_State *L, void* obj, const char* tag) // [-0, +1]
{
	lua_pushlightuserdata(L, obj);       // +1  userdata
	lua_pushlightuserdata(L, (void*)tag);       // +1  type tag
	lua_rawset(L, LUA_REGISTRYINDEX);    // -2
	lua_pushlightuserdata(L, obj);       // +1  return value
}

static void unreg_tagged(lua_State *L, void* obj) // [-0, +0]
{
	lua_pushlightuserdata(L, obj);       // +1  userdata
	lua_pushnil(L);                      // +1  nil
	lua_rawset(L, LUA_REGISTRYINDEX);    // -2
}

static Frame* opt_frame(lua_State *L, int idx, Frame* def) {
	if (lua_isnoneornil(L, idx)) return def;
	return get_tagged(L, idx, c_frame_tag);
}

static Frame* check_frame(lua_State *L, int idx) {
	return get_tagged(L, idx, c_frame_tag);
}

static GfxImage opt_image(lua_State *L, int idx, GfxImage def) {
	if (lua_isnoneornil(L, idx)) return def;
	return get_tagged(L, idx, c_image_tag);
}

static GfxImage check_image(lua_State *L, int idx) {
	return get_tagged(L, idx, c_image_tag);
}


// ------------------------- bindings -------------------------

static int lb_exit_app(lua_State *L) { app_exit(); return 0; }
static int lb_close_doc(lua_State *L) { close_doc(); return 0; }

static int lb_new_doc(lua_State *L)
{
	int width = luaL_checkint(L, 1);
	int height = luaL_checkint(L, 2);
	new_doc(width, height);
	return 0;
}

static int lb_new_layer(lua_State *L)
{
	int above = luaL_optint(L, 1, INT_MAX);
	new_layer(above);
	return 0;
}

static int lb_delete_layer(lua_State *L)
{
	int index = luaL_checkint(L, 1);
	delete_layer(index);
	return 0;
}

static int lb_load_into_layer(lua_State *L)
{
	size_t len;
	int index = luaL_checkint(L, 1);
	const char* path = luaL_checklstring(L, 2, &len);
	stringref s; { s.size = len; s.data = path; }
	load_into_layer(index, s);
	return 0;
}

static int lb_create_frame(lua_State *L)
{
	Frame* parent = opt_frame(L, 1, g_root_frame);
	int after = luaL_optint(L, 2, -1);
	Frame* frame = create_frame(parent, after);
	push_tagged(L, frame, c_frame_tag);
	return 1;
}

static int lb_destroy_frame(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	invalidate_frame(frame);
	destroy_frame(frame);
	return 0;
}

static int lb_insert_frame(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	int after = luaL_optint(L, 2, -1);
	Frame* parent = opt_frame(L, 3, 0);
	invalidate_frame(frame);
	insert_frame(frame, parent, after);
	invalidate_frame(frame);
	return 0;
}

static int lb_set_frame_rect(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	fRect rect;
	rect.left   = (float)luaL_checknumber(L, 2);
	rect.top    = (float)luaL_checknumber(L, 3);
	rect.right  = (float)luaL_checknumber(L, 4);
	rect.bottom = (float)luaL_checknumber(L, 5);
	// after argument validation, set rect.
	frame->message(frame, frameSetRect, &rect);
	invalidate_frame(frame);
	return 0;
}

static int lb_set_frame_col(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	float red   = (float)luaL_checknumber(L, 2);
	float green = (float)luaL_checknumber(L, 3);
	float blue  = (float)luaL_checknumber(L, 4);
	float alpha = (float)luaL_optnumber(L, 5, 1);
	set_frame_col(frame, red, green, blue, alpha);
	invalidate_frame(frame);
	return 0;
}

static int lb_set_frame_alpha(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	float alpha = (float)luaL_checknumber(L, 2);
	set_frame_alpha(frame, alpha);
	invalidate_frame(frame);
	return 0;
}

static int lb_set_frame_image(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	GfxImage image = opt_image(L, 2, 0);
	set_frame_image(frame, image);
	invalidate_frame(frame);
	return 0;
}

static int lb_show_frame(lua_State *L)
{
	Frame* frame = check_frame(L, 1);
	bool show = lua_toboolean(L, 2) ? true : false;
	frame->message(frame, frameSetVisible, &show);
	invalidate_frame(frame);
	return 0;
}

static int lb_load_resource(lua_State *L)
{
	GfxImage image; size_t len;
	const char* path = luaL_checklstring(L, 1, &len);
	stringref s; { s.size = len; s.data = path; }
	image = load_image(s);
	if (image)
		push_tagged(L, image, c_image_tag);
	else
		lua_pushnil(L);
	return 1;
}

static int lb_get_image_info(lua_State *L)
{
	GfxImage image = check_image(L, 1);
	iPair size = (*image)->getSize(image);
	lua_pushinteger(L, size.x);
	lua_pushinteger(L, size.y);
	return 2;
}

static int lb_get_layer_info(lua_State *L)
{
	int index = luaL_checkint(L, 1);
	Frame* layer = get_layer(index);
	if (layer) {
		bool show = false; float alpha = 0; iPair size = {0,0};
		layer->message(layer, frameGetVisible, &show);
		layer->message(layer, frameGetAlpha, &alpha);
		layer->message(layer, frameGetSize, &size);
		lua_pushboolean(L, show);
		lua_pushstring(L, "normal"); // mode
		lua_pushnumber(L, alpha);
		lua_pushinteger(L, size.x);
		lua_pushinteger(L, size.y);
		return 5;
	}
	return 0;
}

static int lb_show_layer(lua_State *L)
{
	int index = luaL_checkint(L, 1);
	Frame* layer = get_layer(index);
	if (layer) {
		bool show = lua_toboolean(L, 2) ? true : false;
		layer->message(layer, frameSetVisible, &show);
		invalidate_all();
	}
	return 0;
}

static int lb_set_brush(lua_State *L)
{
	static const char* modeNames[] = {
		"normal",
		"subtract",
	0};
	static BlendMode modes[] = {
		blendNormal,
		blendSubtract,
	};
	int mode = luaL_checkoption(L, 1, "normal", modeNames);
	int sizeMin = (int)(256.0 * luaL_checknumber(L, 2));
	int sizeMax = (int)(256.0 * luaL_checknumber(L, 3));
	int spacing = (int)(256.0 * luaL_checknumber(L, 4));
	int alphaMin = (int)(256.0 * luaL_checknumber(L, 5));
	int alphaMax = (int)(256.0 * luaL_checknumber(L, 6));
	float red   = (float)luaL_checknumber(L, 7);
	float green = (float)luaL_checknumber(L, 8);
	float blue  = (float)luaL_checknumber(L, 9);
	set_brush_mode(modes[mode]);
	set_brush_size(sizeMin, sizeMax, spacing);
	set_brush_alpha(alphaMin, alphaMax);
	set_brush_col(make_rgba(red, green, blue, 0));
	return 0;
}

static int lb_begin_painting(lua_State *L)
{
	begin_painting();
	return 0;
}

static int lb_active_layer(lua_State *L)
{
	int index = luaL_optint(L, 1, 0);
	active_layer(index);
	return 0;
}

extern UndoBuffer* undoBuf; // TODO: fix this hack.
static int lb_undo(lua_State *L) { undo(undoBuf); return 0; }
static int lb_redo(lua_State *L) { redo(undoBuf); return 0; }

static int lb_zoomIn(lua_State *L) { zoom(1); return 0; }
static int lb_zoomOut(lua_State *L) { zoom(-1); return 0; }
static int lb_zoomHome(lua_State *L) { resetZoom(); return 0; }

static int lb_startTimer(lua_State *L)
{
	long delay    = (long)(1000 * luaL_checknumber(L, 1));
	long interval = (long)(1000 * luaL_optnumber(L, 2, 0));
	if (delay < 0) delay = 0;
	if (interval < 0) interval = 0;
	push_tagged(L, start_timer(delay, interval), c_timer_tag);
	return 1;
}

static int lb_stopTimer(lua_State *L)
{
	void* timer = get_tagged(L, 1, c_timer_tag);
	stop_timer(timer);
	unreg_tagged(L, timer); // pointer now invalid.
	return 0;
}

static const luaL_Reg api_funcs[] = {
  {"exit_app", lb_exit_app},
  {"close_doc", lb_close_doc},
  {"new_doc", lb_new_doc},
  {"new_layer", lb_new_layer},
  {"delete_layer", lb_delete_layer},
  {"load_into_layer", lb_load_into_layer},
  {"CreateFrame", lb_create_frame},
  {"destroy_frame", lb_destroy_frame},
  {"insert_frame", lb_insert_frame},
  {"set_frame_rect", lb_set_frame_rect},
  {"set_frame_col", lb_set_frame_col},
  {"set_frame_image", lb_set_frame_image},
  {"SetFrameAlpha", lb_set_frame_alpha},
  {"show_frame", lb_show_frame},
  {"load_resource", lb_load_resource},
  {"get_image_info", lb_get_image_info},
  {"get_layer_info", lb_get_layer_info},
  {"show_layer", lb_show_layer},
  {"set_brush", lb_set_brush},
  {"begin_painting", lb_begin_painting},
  {"active_layer", lb_active_layer},
  {"undo", lb_undo},
  {"redo", lb_redo},
  {"zoomIn", lb_zoomIn},
  {"zoomOut", lb_zoomOut},
  {"zoomHome", lb_zoomHome},
  {"StartTimer", lb_startTimer},
  {"StopTimer", lb_stopTimer},
  {NULL, NULL}
};

static int setup_lua(lua_State *L)
{
	dataBuf buf;

	// open all standard libraries.
	luaL_openlibs(L);

	// register our print function.
	lua_pushcclosure(L, print, 0);
	lua_setglobal(L, "print");

	// register skunkpad API functions.
	luaL_register(L, "_G", api_funcs);

	// load main script from resource.
	buf = res_load(c_script);
	if (!buf.size) luaL_error(L, "cannot find script");
	if (luaL_loadbuffer(L, (char*)buf.data, buf.size,
		strr_cstr(c_script)) != 0) { buf.free(&buf); lua_error(L); }
	buf.free(&buf);

	// run the main script.
	lua_call(L, 0, 0);

	return 0;
}

void init_bindings()
{
	g_lua = luaL_newstate();
	if (g_lua) {
		// run setup_lua in protected context and report any error.
		lua_pushcclosure(g_lua, setup_lua, 0);
		report(g_lua, docall(g_lua, 0, 1));
	}
}

void term_bindings()
{
	lua_close(g_lua);
	g_lua = 0;
}

void notify_resize(int w, int h)
{
	lua_State *L = g_lua;
	if (L) {
		lua_getglobal(L, "notify_resize");
		lua_pushinteger(L, w);
		lua_pushinteger(L, h);
		report(L, docall(L, 2, 1));
	}
}

void notify_pointer(int x, int y, int buttons)
{
	lua_State *L = g_lua;
	if (L) {
		lua_getglobal(L, "notify_pointer");
		lua_pushinteger(L, x);
		lua_pushinteger(L, y);
		lua_pushinteger(L, buttons);
		report(L, docall(L, 3, 1));
	}
}

void notify_key(int key, int down)
{
	lua_State *L = g_lua;
	if (L) {
		lua_getglobal(L, "notify_key");
		lua_pushinteger(L, key);
		lua_pushboolean(L, down);
		report(L, docall(L, 2, 1));
	}
}

void notify_timer(void* timer)
{
	lua_State *L = g_lua;
	if (L) {
		lua_getglobal(L, "notify_timer");
		push_tagged(L, timer, c_timer_tag);
		report(L, docall(L, 1, 1));
	}
}

void unreg_frame(Frame* frame)
{
	unreg_tagged(g_lua, frame);
}

void unreg_timer(void* timer)
{
	unreg_tagged(g_lua, timer);
}
