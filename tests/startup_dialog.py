"""Drive the real native startup chooser in isolated Windows/X11 launches."""
from pathlib import Path
import ctypes as c, json, os, shutil, subprocess, time
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('startup-dialog-'+platform);run.mkdir(exist_ok=True)
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':
    local=run/'OpenMR2.exe';shutil.copy2(exe,local);exe=local
    u=c.windll.user32
    for name in ('FindWindowA','GetDlgItem','FindWindowExA'):getattr(u,name).restype=c.c_void_p
    u.GetWindowThreadProcessId.argtypes=[c.c_void_p,c.POINTER(c.c_ulong)]
    u.GetWindowTextA.argtypes=[c.c_void_p,c.c_char_p,c.c_int]
    u.GetClassNameA.argtypes=[c.c_void_p,c.c_char_p,c.c_int]
    u.GetDlgItem.argtypes=[c.c_void_p,c.c_int];u.SendMessageA.argtypes=[c.c_void_p,c.c_uint,c.c_size_t,c.c_ssize_t]
    u.PostMessageA.argtypes=[c.c_void_p,c.c_uint,c.c_size_t,c.c_ssize_t]
    def windows():
        found=[];callback=c.WINFUNCTYPE(c.c_bool,c.c_void_p,c.c_ssize_t)
        def visit(hwnd,_):
            if u.GetWindowThreadProcessId(hwnd,c.byref(pid:=c.c_ulong())) and pid.value==process.pid:
                title=c.create_string_buffer(256);kind=c.create_string_buffer(256);u.GetWindowTextA(hwnd,title,256);u.GetClassNameA(hwnd,kind,256);found.append((hwnd,title.value.decode('latin1'),kind.value.decode('latin1')))
            return True
        u.EnumWindows(callback(visit),0);return found
    def click(hwnd,which):u.SendMessageA(u.GetDlgItem(hwnd,which),0x00F5,0,0)
    def stop(hwnd):u.PostMessageA(hwnd,0x10,0,0)
else:
    x=c.CDLL('libX11.so.6');x.XOpenDisplay.restype=c.c_void_p;x.XDefaultRootWindow.argtypes=[c.c_void_p];x.XDefaultRootWindow.restype=c.c_ulong
    display=x.XOpenDisplay(None);assert display,'Run with xvfb-run -a'
    rootwin=x.XDefaultRootWindow(display)
    x.XQueryTree.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_ulong),c.POINTER(c.POINTER(c.c_ulong)),c.POINTER(c.c_uint)]
    x.XFetchName.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(c.c_void_p)];x.XFree.argtypes=[c.c_void_p];x.XFlush.argtypes=[c.c_void_p]
    x.XSendEvent.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_long,c.c_void_p]
    class Button(c.Structure):_fields_=[('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),('display',c.c_void_p),('window',c.c_ulong),('root',c.c_ulong),('subwindow',c.c_ulong),('time',c.c_ulong),('x',c.c_int),('y',c.c_int),('x_root',c.c_int),('y_root',c.c_int),('state',c.c_uint),('button',c.c_uint),('same_screen',c.c_int)]
    def windows():
        r=c.c_ulong();p=c.c_ulong();children=c.POINTER(c.c_ulong)();n=c.c_uint();x.XQueryTree(display,rootwin,c.byref(r),c.byref(p),c.byref(children),c.byref(n));found=[]
        for i in range(n.value):
            name=c.c_void_p();x.XFetchName(display,children[i],c.byref(name));title=c.string_at(name).decode('latin1') if name.value else '';found.append((children[i],title,''));
            if name.value:x.XFree(name)
        if children:x.XFree(children)
        return found
    def click(hwnd,which):
        px,py={101:(100,80),102:(360,80),103:(30,140)}[which];event=(c.c_long*24)();b=c.cast(event,c.POINTER(Button)).contents;b.type=4;b.send_event=1;b.display=display;b.window=hwnd;b.root=rootwin;b.x=px;b.y=py;b.button=1;b.same_screen=1;x.XSendEvent(display,hwnd,0,4,event);x.XFlush(display)
    class Client(c.Structure):_fields_=[('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),('display',c.c_void_p),('window',c.c_ulong),('message_type',c.c_ulong),('format',c.c_int),('data',c.c_long*5)]
    x.XInternAtom.argtypes=[c.c_void_p,c.c_char_p,c.c_int];x.XInternAtom.restype=c.c_ulong
    def stop(hwnd):
        event=(c.c_long*24)();m=c.cast(event,c.POINTER(Client)).contents;m.type=33;m.display=display;m.window=hwnd;m.message_type=x.XInternAtom(display,b'WM_PROTOCOLS',0);m.format=32;m.data[0]=x.XInternAtom(display,b'WM_DELETE_WINDOW',0);x.XSendEvent(display,hwnd,0,0,event);x.XFlush(display)

def wait_window(title=None,kind=None,timeout=25):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        assert process.poll() is None,f'game exited with {process.returncode}'
        for hwnd,text,cls in windows():
            if (title is None or text==title) and (kind is None or cls==kind):return hwnd
        time.sleep(.05)
    raise AssertionError(f'No window {title} {kind}')
def launch():
    kwargs={}
    if os.name=='nt':
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
    return subprocess.Popen([str(exe),'--game',str(root.parents[1]/'Megarace 2 English')],cwd=run,**kwargs)
def finish():
    if process.poll() is None:
        if os.name=='nt':
            for hwnd,title,kind in windows():
                if kind=='OpenMR2Window':stop(hwnd)
        else:process.terminate()
    try:process.wait(timeout=10)
    except subprocess.TimeoutExpired:process.kill();process.wait()
def mode_window():return wait_window('OpenMR2 - Startup mode')
def game_window():return wait_window(kind='OpenMR2Window') if os.name=='nt' else wait_window(next(t for _,t,_ in windows() if t.startswith('OpenMR2 ') and t!='OpenMR2 - Startup mode'))
receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
cfg.write_text(receipt+'\nremember_startup_mode = 0\nstartup_mode = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
results=[]
try:
    process=launch();hwnd=mode_window()
    assert not any(kind=='OpenMR2Window' for _,_,kind in windows())
    if os.name=='nt':
        labels=[]
        for child in (101,102,103):
            label=c.create_string_buffer(200);u.GetWindowTextA(u.GetDlgItem(hwnd,child),label,200);labels.append(label.value.decode())
        assert labels==['Revived Mode','Retro Mode','Remember my choice'],labels
    results.append('English native chooser precedes game window')
    click(hwnd,102);time.sleep(.8)
    text=cfg.read_text();assert 'startup_mode = 1' in text and 'remember_startup_mode = 0' in text
    if os.name=='nt':game_window()
    finish();results.append('Unchecked Retro selection is applied and saved without suppressing next prompt')
    process=launch();hwnd=mode_window();click(hwnd,103);click(hwnd,101);time.sleep(.8)
    assert 'startup_mode = 0' in cfg.read_text() and 'remember_startup_mode = 1' in cfg.read_text()
    if os.name=='nt':game_window()
    finish();results.append('Remember checkbox saves Revive choice')
    process=launch();time.sleep(2)
    assert process.poll() is None and not any(t=='OpenMR2 - Startup mode' for _,t,_ in windows())
    if os.name=='nt':game_window()
    finish();results.append('Next launch skips remembered chooser')
    cfg.write_text(cfg.read_text().replace('remember_startup_mode = 1','remember_startup_mode = 0'))
    if os.name=='nt':
        process=launch();hwnd=mode_window();u.PostMessageA(u.GetDlgItem(hwnd,101),0x0100,13,0);time.sleep(.4)
        assert not any(t=='OpenMR2 - Startup mode' for _,t,_ in windows()),'Enter must activate the focused mode button'
        game_window();finish();results.append('Keyboard Enter starts the focused mode')
    process=launch();hwnd=mode_window();before=cfg.read_bytes();stop(hwnd);process.wait(timeout=10);assert before==cfg.read_bytes();results.append('Closing chooser cancels startup without rewriting settings')
finally:
    if 'process' in globals() and process.poll() is None:process.kill();process.wait()
(root/'build'/f'{platform}-startup-dialog-results.json').write_text(json.dumps({'platform':platform,'checks':results,'failures':0},indent=2))
print(platform,':',len(results),'native startup dialog checks passed',flush=True)
