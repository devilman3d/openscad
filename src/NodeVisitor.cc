#include "NodeVisitor.h"
#include "state.h"

State NodeVisitor::nullstate(nullptr);

Response NodeVisitor::traverse(const AbstractNode &node, const State &state)
{
	State nodeState = state;
	nodeState.setNumChildren(node.getChildren().size());
	nodeState.setPrefix(true);
	
	Response response = node.accept(nodeState, *this);

	// Pruned traversals mean don't traverse children
	if (response == ContinueTraversal) {
		for(const auto &chnode : node.getChildren()) {
			State chstate = nodeState;
			chstate.setParent(&node, nodeState);
			response = this->traverse(*chnode, chstate);
			if (response == AbortTraversal) 
				break; // Abort immediately
		}
	}

	// Postfix is executed for all non-aborted traversals
	if (response != AbortTraversal) {
		nodeState.setPostfix(true);
		response = node.accept(nodeState, *this);
	}

	// continue traversing siblings if not aborted
	if (response != AbortTraversal) 
		response = ContinueTraversal;

	return response;
}
