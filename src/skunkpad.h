#ifndef SKUNKPAD_H
#define SKUNKPAD_H

#include "str.h"
#include "surface.h"
#include "frames.h"

// api for lua bindings.
string browse_file_open(stringref caption, stringref path);
string browse_file_save(stringref caption, stringref path);
void new_doc(int width, int height);
void open_doc(stringref path);
void save_doc(stringref path);
void close_doc();
void new_layer(int above);
void delete_layer(int index);
void show_layer(int index, bool show);
void load_into_layer(int index, stringref path);
void active_layer(int index); // for painting.
void set_brush_mode(BlendMode mode);
void set_brush_size(int sizeMin, int sizeMax, int spacing);
void set_brush_alpha(int alphaMin, int alphaMax);
void set_brush_col(RGBA col);
void begin_painting();
void invalidate_all();
void zoom(int steps);
void resetZoom();

// timers
void* start_timer(unsigned long delay, unsigned long interval);
void stop_timer(void* timer);


// overlay frames.

Frame* create_frame(Frame* parent, int after);
void destroy_frame(Frame* frame);
void insert_frame(Frame* frame, Frame* parent, int after);
void set_frame_col(Frame* frame, float red, float green, float blue, float alpha);
void set_frame_alpha(Frame* frame, float alpha);
void set_frame_image(Frame* frame, GfxImage image);
void invalidate_frame(Frame* frame);
GfxImage load_image(stringref path);
Frame* get_layer(int index);

extern Frame* g_root_frame;


/*
void move_frame(Frame* frame, float left, float top, float right, float bottom);
void set_frame_colour(Frame* frame, float r, float g, float b, float a);
void set_frame_image(Frame* frame, Image* image);
void set_frame_coords(Frame* frame, float left, float top, float right, float bottom);
void set_frame_border(int frame, float thickness, float dash, float r, float g, float b, float a);
void set_frame_shape(int frame, float radius); // non-zero for round rect.
*/


// undo

typedef struct UndoBuffer UndoBuffer;
typedef void (*UndoBufferApply)(void* obj, void* data, int size);
UndoBuffer* undobuf_create(size_t size);
void undobuf_resize(UndoBuffer* ub, size_t size);
void undobuf_begin(UndoBuffer* ub, void* data, int size); // begin a packet.
void undobuf_append(UndoBuffer* ub, void* data, int size); // append to packet.
void undobuf_end(UndoBuffer* ub); // end of current packet.
void undobuf_undo(UndoBuffer* ub, UndoBufferApply func, void* obj);
void undobuf_redo(UndoBuffer* ub, UndoBufferApply func, void* obj);

void undo(UndoBuffer* ub);
void redo(UndoBuffer* ub);

// app stuff that undo uses...
extern Frame* activeLayer;
void update_brush();


// brush engine.
struct Tablet_InputEvent;
typedef struct DabPainter DabPainter;
// merge deferred painting into the document.
// org: top-left corner of source rect (size from bounds)
// bounds: destination rect in document space.
typedef void (*DabPainterOutput)(void* obj, SurfaceData* image, iPair org, iRect bounds);
DabPainter* createDabPainter(GfxDraw draw);
//void painter_set_context(DabPainter* dp, GfxContext cx);
void painter_begin(DabPainter* dp, struct Tablet_InputEvent* e);
void painter_begin_batch(DabPainter* dp);
void painter_draw(DabPainter* dp, struct Tablet_InputEvent* e);
void painter_end_batch(DabPainter* dp);
void painter_end(DabPainter* dp);
void painter_set_brush(DabPainter* dp, SurfaceData* brush);
void painter_set_colour(DabPainter* dp, RGBA col);
void painter_set_alpha_range(DabPainter* dp, int min, int max);
// specify minimum and maximum brush size in Q8 document coords.
// the brush shape is always square for now.
void painter_set_size_range(DabPainter* dp, int min, int max);
// specify dab spacing in Q8 document coords.
void painter_set_spacing(DabPainter* dp, int ratio);
// set a callback that will merge deferred paint into the document.
void painter_set_output(DabPainter* dp, DabPainterOutput func, void* obj);
//GfxImage painter_get_accum(DabPainter* dp);
//RGBA painter_get_col(DabPainter* dp);


// bindings
void init_bindings();
void term_bindings();
void notify_resize(int w, int h);
void notify_pointer(int x, int y, int buttons);
void notify_wheel(int x, int y, int delta);
void notify_scroll(int x, int y);
void notify_key(int key, int down);
void notify_timer(void* timer);
void unreg_frame(Frame* frame);
void unreg_timer(void* timer);


// utils
RGBA make_rgba(float r, float g, float b, float a);


#endif
