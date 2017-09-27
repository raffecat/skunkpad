#ifndef CPART_GL_VIEW
#define CPART_GL_VIEW

// GLView - child UI Window for OpenGL rendering.
// Depends on: ui, glrender

import graphics;
import ui;

export type GLViewSceneFunc = fun (void* data) -> void;

export interface GLView {

    void glview_resize(int width, int height);
    //void glview_set_scene(, GfxScene scene);
    void glview_fullscreen();
    void glview_windowed();
    void glview_render();
    void glview_destroy();
    graphics.GfxContext glview_get_context();

    void glview_set_scene(GLViewSceneFunc scene, void* data);
};

export GLView* glview_create(ref ui.Window parent, ui.EventFunc events, void* context);
