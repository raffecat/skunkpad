
def MouseOverTracker(Node& rootNode)
{
	// dependencies:
	// known: root node containing all mouse-tracked nodes.
	// input: mouse update event { position, button down flag }
	// enum: parent node of any located node.
	// enum: all children of any located node.
	// query: hit test any located node.
	// send: messages to any located node.
	// state: active node, button down flag.

	// assumptions:
	// hit areas of children are enclosed within the hit area of the parent.
	// hit testing is always done on absolute coordinates (not good)

	// ordering:
	// mouse up -> leave -> enter -> mouse down

	Node& overNode
	bool down

	on mouseUpdate(event)
	{
		// generate any mouse-up at previous position.
		if (down && !event.down) {
			overNode <- up()
			down = false
		}
		// leave nodes that the mouse is no longer over.
		while (!overNode.hitTest(event.position)) {
			overNode <- leave(down)
			if (overNode == rootNode)
				break
			overNode = overNode.parentNode
		}
		// enter nodes that the mouse is newly over.
		do {
			bool found = false
			for (child in overNode) {
				if (child.hitTest(event.position)) {
					child <- enter(down)
					overNode = child
					found = true
					break
				}
			}
		} while (found)
		// generate any mouse-down at new position.
		if (!down && event.down) {
			overNode <- down()
			down = true
		}
	}
}
