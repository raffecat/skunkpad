// Graph nodes for design time.

#include "defs.h"
#include "graphics.h"
#include "affine.h"

// Shape:
// rectangle, circle, or winding. (capsule, cone; line-arc?)
// island detection: sets of (aabb) overlapping shapes.
// winding: pairs (x,y) of coords representing a convex hull in
// counter-clockwise (+ve length) or clockwise (-ve length) order.
typedef struct GfxShape GfxShape;
typedef enum GfxShapeFlags {
	gfxShapeRect   = 0x00000000, // left, top, right, bottom (no length)
	gfxShapeCircle = 0x10000000, // x, y, radius (no length)
	gfxShapeCCW    = 0x20000000, // counter-clockwise winding (length in pairs)
	gfxShapeCW     = 0x30000000, // clockwise winding (length in pairs)
	gfxShapeMask   = 0xF0000000, // (shape & gfxShapeMask) == gfxShapeCircle
	gfxLengthMask  = 0x00FFFFFF, // length = (shape & gfxLengthMask)
} GfxShapeFlags;
struct GfxShape {
	uint32 type; // shape type, number of pairs.
	float p[1];
};
#define GfxShapeType(type) ((type) & gfxShapeMask)
#define GfxShapeLength(type) ((type) & gfxLengthMask)

// Path:
// array of commands and arguments representing a drawing path.
typedef struct GfxPath GfxPath;
typedef enum GfxPathCmd {
	gfxPathEnd = 0,
	gfxPathMoveTo = 1, // (x,y)
	gfxPathLineTo = 2, // (x,y)
	gfxPathQuadratic = 3, // control (x,y), end (x,y)
} GfxPathCmd;
typedef union GfxPathElem {
	GfxPathCmd cmd;
	float v;
} GfxPathElem;
struct GfxPath {
	unsigned int length;
	GfxPathElem elems[1];
};

// Graphic:
// a path filled with a transformed image.
typedef struct GfxGraphic GfxGraphic;
struct GfxGraphic {
	Affine2D transform; // image transform.
	GfxImage image; // fill image; can be procedural (e.g. gradient)
	GfxPath* path;
};

typedef struct SGHitTest SGHitTest;

typedef struct SGContext SGContext;
typedef struct SGNode SGNode;
typedef struct SGTransform SGTransform;
typedef struct SGInstance SGInstance;
typedef struct SGGraphic SGGraphic;
typedef struct SGHull SGHull;
typedef struct SGTile SGTile;

typedef struct SGTileSheet SGTileSheet; // tile-sheet resource.
typedef struct SGTemplate SGTemplate; // graph template resource.

typedef struct SGNodeType {
	// plain old object model
	void (*update)(SGNode* node);
	void (*render)(SGNode* node, GfxRender draw);
	void (*hitTest)(SGNode* node, SGHitTest* hit);
	// animation properties
	void (*position)(SGNode* node, float x, float y);
	void (*angle)(SGNode* node, float a);
	void (*scale)(SGNode* node, float x, float y);
	void (*colour)(SGNode* node, float r, float g, float b);
	void (*alpha)(SGNode* node, float a);
	void (*visible)(SGNode* node, bool show);
	// create, destroy
	void (*spawn)(SGNode* node);
	void (*destroy)(SGNode* node);
	// designer support
	void (*renderProto)(SGNode* node, GfxRender draw);
} SGNodeType;

#define SGNode_render(self, draw) (self)->type->render((self),(draw))

struct SGContext { GfxContext cx; };

// base structure for all graph nodes.
struct SGNode { SGNodeType* type; size_t refs; };

// common fields for all transform nodes.
struct SGTransform { SGNode node; Affine2D t; fRect aabb; };

// an instance of a template resource.
struct SGInstance { SGTransform tx; SGTemplate* rsrc; int numNodes; SGNode** nodes; };

// render a list of path-based graphics.
struct SGGraphic { SGTransform tx; int numGfx; GfxGraphic** gfx; };

// collision hull.
struct SGHull { SGTransform tx; int numShapes; GfxShape** shapes; };

// draw one tile from a tile-sheet.
struct SGTile { SGTransform tx; SGTileSheet* ts; int frame; };

SGInstance* sgCreateInstance(SGContext* sgc);


// SGInstance

static void sgInst_update(SGNode* node) {}

static void sgInst_render(SGNode* node, GfxRender draw) {
	SGInstance* n = (SGInstance*)node;
	int i = 0, count = n->numNodes;
	SGNode** nodes = n->nodes;
	for (; i < count; i++) {
		nodes[i]->type->render(nodes[i], draw);
	}
}

static void sgInst_hitTest(SGNode* node, SGHitTest* hit) {}

static SGNodeType sg_instance = {
	sgInst_update,
	sgInst_render,
	sgInst_hitTest,
};

static const fRect c_empty_rect = {0,0,0,0};

SGInstance* sgCreateInstance(SGContext* sgc)
{
	SGInstance* n = cpart_new(SGInstance);
	n->tx.node.type = &sg_instance;
	n->tx.node.refs = 1;
	n->tx.aabb = c_empty_rect;
	n->tx.t = c_affine2d_identity;
	n->rsrc = 0; // TODO.
	n->numNodes = 0;
	n->nodes = 0;
	return n;
}
