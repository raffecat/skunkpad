#include "defs.h"
#include "sg.h"
#include "graphics.h"


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
