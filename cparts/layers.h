#ifndef CPART_LAYERS
#define CPART_LAYERS

// Stacking Layers library

typedef struct Layer Layer;
typedef struct SurfaceLayer SurfaceLayer;
typedef struct RenderRequest RenderRequest;
typedef struct LayerEvent LayerEvent;

typedef void (*SurfaceLayerRenderFunc)(Layer* layer, RenderRequest* req);
typedef void (*SurfaceLayerEventFunc)(Layer* layer, LayerEvent* e);

SurfaceLayer* surface_layer_create(size_t width, size_t height);


#endif
