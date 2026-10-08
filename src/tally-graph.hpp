#pragma once

#include <unordered_set>
#include <vector>

namespace mv {
// enumerate(node, visitor) visits each edge as visitor(child, visible).
// Visibility belongs to an edge (scene item), not globally to a source.
template<typename Node, typename Enumerate, typename Match>
bool graphContainsVisible(Node root, Node target, Enumerate enumerate, Match match)
{
    if (!root || !target)
        return false;
    // Iterative traversal avoids stack exhaustion with deeply nested scenes.
    // A bounded query fails closed rather than guessing an on-air camera.
    std::unordered_set<Node> visited;
    std::vector<Node> pending{root};
    while (!pending.empty()) {
        Node node = pending.back(); pending.pop_back();
        if (!node || !visited.insert(node).second) continue;
        if (visited.size() > 8192) return false;
        if (match(node, target)) return true;
        enumerate(node, [&](Node child, bool visible) {
            if (visible && child && !visited.count(child)) pending.push_back(child);
        });
    }
    return false;
}

template<typename Node, typename Enumerate>
bool graphContainsVisible(Node root, Node target, Enumerate enumerate)
{
    return graphContainsVisible(root, target, enumerate, [](Node a, Node b) { return a == b; });
}

// Only one distinct visible video leaf may identify an otherwise absent camera
// scene. Repeated references to the same camera do not make it ambiguous.
template<typename Node, typename Enumerate, typename IsVideoLeaf>
Node graphUniqueVisibleLeaf(Node root, Enumerate enumerate, IsVideoLeaf isVideoLeaf)
{
    std::unordered_set<Node> visited;
    std::unordered_set<Node> leaves;
    std::vector<Node> pending{root};
    while (!pending.empty()) {
        Node node = pending.back(); pending.pop_back();
        if (!node || !visited.insert(node).second) continue;
        if (visited.size() > 8192) return Node{};
        if (isVideoLeaf(node)) {
            leaves.insert(node);
            if (leaves.size() > 1) return Node{};
            continue;
        }
        enumerate(node, [&](Node child, bool visible) {
            if (visible && child && !visited.count(child)) pending.push_back(child);
        });
    }
    return leaves.size() == 1 ? *leaves.begin() : Node{};
}

template<typename Node, typename Enumerate, typename IsContainer, typename IsVideoLeaf, typename Match>
bool graphSourceOnRoot(Node root, Node target, Node explicitTallySource,
                       Enumerate enumerate, IsContainer isContainer, IsVideoLeaf isVideoLeaf, Match match)
{
    if (explicitTallySource)
        return graphContainsVisible(root, explicitTallySource, enumerate, match);
    if (graphContainsVisible(root, target, enumerate, match))
        return true;
    if (!target || !isContainer(target))
        return false;
    const auto leaf = graphUniqueVisibleLeaf(target, enumerate, isVideoLeaf);
    return leaf && graphContainsVisible(root, leaf, enumerate, match);
}
template<typename Node, typename Enumerate, typename IsContainer, typename IsVideoLeaf>
bool graphSourceOnRoot(Node root, Node target, Node explicitTallySource,
                       Enumerate enumerate, IsContainer isContainer, IsVideoLeaf isVideoLeaf)
{
    return graphSourceOnRoot(root, target, explicitTallySource, enumerate, isContainer, isVideoLeaf,
                             [](Node a, Node b) { return a == b; });
}
} // namespace mv
