"""Use real libobs copy/pixel fixtures to validate Multiview's exact native path code."""
from pathlib import Path
import os
base=Path(os.environ['MV_HYBRID_NATIVE_FIXTURE']).resolve()
code=base.read_text(encoding='utf-8-sig')
# The reference fixture itself injects native tests into native-groups.py. Add
# our checks to its probe so all the same source/scene copies remain alive.
marker="    print('PASS Hybrid copy lifetime: 500 clones, allocations', before, '->', after)"
addition='''    multiview=C.CDLL(os.environ['MV_HYBRID_NATIVE_PROBE'])
    on=multiview.mv_test_source_on_root;on.restype=C.c_bool;on.argtypes=[P,P,P]
    scene1=scene_source(extra_scenes[0]);scene2=scene_source(extra_scenes[1])
    assert on(me1_copy,scene1,None) and not on(me1_copy,scene2,None)
    assert on(me1_copy,camera,None) and on(me1_copy,scene1,camera)
    assert not on(me1_copy,scene1,gray)
    assert on(me2_copy,gray,None)
    fake=scene_create(b'MixView1');extra_scenes.append(fake)
    assert not on(me2_copy,scene_source(fake),None), 'Internal private view aliased by name'
    # Original Preview hides a camera; frozen PGM remains visible.
    camera_item=api('obs_scene_find_source',P,P,C.c_char_p)(extra_scenes[0],b'Camera')
    api('obs_sceneitem_set_visible',None,P,C.c_bool)(camera_item,False)
    assert on(me1_copy,camera,None)
    assert not on(scene1,camera,None)
    for _ in range(100):assert on(me1_copy,scene1,None)
    api('obs_wait_for_destroy_queue',C.c_bool)();baseline=allocations()
    for _ in range(2000):assert on(me1_copy,scene1,None)
    api('obs_wait_for_destroy_queue',C.c_bool)();final=allocations()
    assert final==baseline,(baseline,final)
    print('PASS Multiview native: ME1 scene/source/explicit mapping, ME2, internal-name collision, frozen visibility, 2000 queries',baseline,'->',final,flush=True)
'''
assert marker in code
code=code.replace(marker,marker+'\n'+addition)
exec(compile(code,str(base),'exec'),{'__file__':str(base),'__name__':'__main__'})
