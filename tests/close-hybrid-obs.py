"""Normal close for the exact portable OBS process created by our Hybrid test."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import sys
pid=int(sys.argv[1]);expected=Path(sys.argv[2]).resolve()
assert expected==(Path(__file__).resolve().parents[1]/'.local/hybrid-runtime/obs/bin/64bit/obs64.exe').resolve()
k=C.WinDLL('kernel32',use_last_error=True);u=C.WinDLL('user32',use_last_error=True)
k.OpenProcess.restype=W.HANDLE;k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD]
h=k.OpenProcess(0x1000,False,pid);assert h
try:
    buffer=C.create_unicode_buffer(32768);size=W.DWORD(len(buffer))
    k.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)]
    assert k.QueryFullProcessImageNameW(h,0,buffer,C.byref(size))
    assert Path(buffer.value).resolve()==expected
finally:
    k.CloseHandle.argtypes=[W.HANDLE];k.CloseHandle(h)
windows=[];titles=[]
callback=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
@callback
def visit(hwnd,_):
    owner=W.DWORD();u.GetWindowThreadProcessId(hwnd,C.byref(owner))
    if owner.value==pid:
        title=C.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,title,1024)
        titles.append(title.value)
        if title.value.startswith('OBS ') and not title.value.startswith('OBS Multiview Plus'):windows.append(hwnd)
    return True
u.EnumWindows.argtypes=[callback,W.LPARAM];u.EnumWindows(visit,0)
assert len(windows)==1, str(titles)
u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
assert u.PostMessageW(windows[0],0x10,0,0)
