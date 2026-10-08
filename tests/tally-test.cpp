#include "tally-graph.hpp"
#include <iostream>
#include <vector>
#include <utility>

struct Node {
    std::vector<std::pair<Node *, bool>> children;
    bool container = false;
    bool video = true;
};

static bool linked(Node *root, Node *target, Node *explicitSource = nullptr)
{
    return mv::graphSourceOnRoot(root, target, explicitSource, [](Node *node, auto visit) {
        for (auto [child, shown] : node->children)
            visit(child, shown);
    }, [](Node *node) { return node->container; },
    [](Node *node) { return node->video && !node->container; });
}

static bool visible(Node *root, Node *target)
{
    return mv::graphContainsVisible(root, target, [](Node *node, auto visit) {
        for (auto [child, shown] : node->children)
            visit(child, shown);
    });
}

int main()
{
    int failed = 0;
    int total = 0;
    auto check = [&](bool condition, const char *scenario) {
        ++total;
        if (!condition) {
            std::cerr << "FAIL: " << scenario << '\n';
            ++failed;
        }
    };
    Node camera, graphic, nested, group, program, preview;
    nested.children = {{&camera, true}};
    group.children = {{&nested, true}};
    program.children = {{&group, true}, {&graphic, false}};
    preview.children = {{&graphic, true}};
    check(visible(&program, &camera), "visible nested scene inside group");
    check(!visible(&program, &graphic), "hidden direct item");
    check(visible(&preview, &graphic), "independent preview path");
    check(visible(&program, &program), "root scene itself");
    program.children[0].second = false;
    check(!visible(&program, &camera), "hidden parent suppresses descendants");
    program.children.push_back({&nested, true});
    check(visible(&program, &camera), "other visible instance survives hidden branch");
    nested.children[0].second = false;
    check(!visible(&program, &camera), "hidden leaf under visible scene");
    nested.children[0].second = true;
    camera.children.push_back({&program, true});
    check(!visible(&program, &graphic), "cycle terminates without false tally");
    check(visible(&program, &camera) || visible(&preview, &camera), "transition outgoing source");
    check(visible(&program, &graphic) || visible(&preview, &graphic), "transition incoming source");
    check(!visible(nullptr, &camera), "missing root");
    check(!visible(&program, nullptr), "missing source");

    Node cam01, cam02, logo, audio;
    audio.video = false;
    Node cameraScene{{{&cam02, true}}, true};
    Node pgmGroup{{{&cam01, false}, {&cam02, true}}, true};
    Node pgmB{{{&pgmGroup, true}}, true};
    Node sceneGroup{{{&cameraScene, true}}, true};
    Node pgmA{{{&sceneGroup, true}}, true};
    check(linked(&pgmA, &cameraScene), "camera scene nested in PGM group");
    check(linked(&pgmB, &cameraScene), "camera leaf used directly by PGM group");
    check(linked(&pgmA, &cam02), "source tile follows nested scene leaf");
    check(linked(&pgmB, &cam02), "source tile follows direct group leaf");
    check(!linked(&pgmB, &cam01), "other camera hidden in PGM group");
    pgmB.children[0].second = false;
    check(!linked(&pgmB, &cameraScene), "hidden PGM group suppresses automatic link");
    pgmB.children[0].second = true;
    cameraScene.children[0].second = false;
    check(!linked(&pgmB, &cameraScene), "hidden camera inside absent camera scene cannot link");
    cameraScene.children[0].second = true;
    cameraScene.children.push_back({&cam02, true});
    check(linked(&pgmB, &cameraScene), "duplicate camera leaves remain unique by source");
    cameraScene.children.push_back({&audio, true});
    check(linked(&pgmB, &cameraScene), "audio leaf ignored for unique video source");
    cameraScene.children.push_back({&logo, true});
    check(!linked(&pgmB, &cameraScene), "composite camera plus logo requires explicit target");
    Node logoOnlyProgram{{{&logo, true}}, true};
    check(!linked(&logoOnlyProgram, &cameraScene), "shared logo cannot tally composite camera scene");
    check(linked(&pgmB, &cameraScene, &cam02), "explicit camera overrides ambiguous leaves");
    check(!linked(&logoOnlyProgram, &cameraScene, &cam02), "explicit camera ignores shared logo");
    check(!linked(&pgmA, &cameraScene, &cam01), "explicit target replaces direct scene matching");
    check(linked(&pgmA, &cameraScene), "direct composite scene remains on air");
    cameraScene.children.push_back({&cam01, true});
    check(!linked(&pgmB, &cameraScene), "multiple cameras cannot choose arbitrary leaf");
    check(linked(&pgmA, &cam02) || linked(&logoOnlyProgram, &cam02), "transition linked outgoing camera");
    check(linked(&logoOnlyProgram, &cameraScene, &cam02) || linked(&pgmB, &cameraScene, &cam02), "transition linked incoming camera");
    Node originalLeaf, clonedLeaf;
    Node originalScene{{{&originalLeaf,false}},true}, clonedScene{{{&clonedLeaf,true}},true};
    auto cloneMatch = [&](Node *a, Node *b) {
        return a == b || (a == &clonedLeaf && b == &originalLeaf) || (a == &clonedScene && b == &originalScene);
    };
    auto cloneOn = [&](Node *wanted) {
        return mv::graphContainsVisible(&clonedScene,wanted,[](Node *n,auto visit){
            for (auto [child,shown]:n->children) visit(child,shown);
        },cloneMatch);
    };
    check(cloneOn(&originalScene), "clone aliases the camera scene identity");
    check(cloneOn(&originalLeaf), "clone aliases source even after Preview hides original");
    clonedScene.children[0].second=false; originalScene.children[0].second=true;
    check(!cloneOn(&originalLeaf), "clone hidden item never borrows Preview visibility");
    check(!cloneOn(&cam01), "unrelated camera is not promoted by alias");
    std::vector<Node> deep(4096);
    for (size_t i=0; i+1<deep.size(); ++i) deep[i].children={{&deep[i+1],true}};
    check(visible(&deep.front(), &deep.back()), "deep scene graph uses no recursive stack");
    deep.back().children={{&deep.front(),true}};
    check(!visible(&deep.front(), &camera), "deep cycle terminates");
    std::vector<Node> excessive(8193);
    for (size_t i=0; i+1<excessive.size(); ++i) excessive[i].children={{&excessive[i+1],true}};
    check(!visible(&excessive.front(), &excessive.back()), "over-limit graph fails closed");
    if (!failed)
        std::cout << total << " tally scenarios passed\n";
    return failed ? 1 : 0;
}
