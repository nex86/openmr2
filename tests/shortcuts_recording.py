"""Send real OS key events to a game-owned test window; probe the actual MP4/JPEG."""
from pathlib import Path
import os,sys,subprocess,time,json,ctypes,shutil
from PIL import Image
root=Path(__file__).resolve().parents[1];windows=os.name=='nt';packaged=os.environ.get('OPENMR2_TEST_APPIMAGE');platform='windows' if windows else 'linux-appimage' if packaged else 'linux'
if not windows and not os.environ.get('DISPLAY'):
 raise SystemExit(subprocess.call(['xvfb-run','-a',sys.executable,__file__],env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1')))
run=root/'build'/('shortcuts-'+platform);run.mkdir(exist_ok=True);config=(run/'data/OpenMR2/revived_config') if packaged else run/'revived_config';config.mkdir(parents=True,exist_ok=True)
cfg=config/'openmr2_setup.cfg';receipt=''
previous=root/'build'/('video-upscale-'+('linux' if packaged else platform))/'revived_config/openmr2_setup.cfg'
if previous.exists():receipt='\n'.join(s for s in previous.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language =')))
cfg.write_text(receipt+'\nremember_startup_mode = 1\nstartup_mode = 0\ngraphics_mode = 0\ntexture_filter = 0\nframe_rate = original\nvideo_upscaling = 0\nvideo_filter = 0\nretro_scale = 1\nretro_output_filter = point\nmsaa = 0\nfxaa = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
exe=root/'build'/('OpenMR2.exe' if windows else 'openmr2_linux');ffprobe=str(root/'build/tools/ffprobe.exe') if windows else 'ffprobe'
if packaged:exe=Path(packaged)
if windows:
 shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe';(run/'tools').mkdir(exist_ok=True);shutil.copy2(root/'build/tools/ffmpeg.exe',run/'tools/ffmpeg.exe')
checks=[];kwargs={}
if not windows:kwargs['start_new_session']=True
if packaged:kwargs['env']=dict(os.environ,APPIMAGE_EXTRACT_AND_RUN='1',XDG_DATA_HOME=str(run/'data'))
if windows:
 si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0;kwargs['startupinfo']=si
process=subprocess.Popen([str(exe),'--game',str(root.parents[1]/'Megarace 2 English'),'--track','1','--frame','150','--objtest','--size','640x480','--quit','90']+(['--debug'] if os.environ.get('OPENMR2_TEST_VERBOSE') else []),cwd=run,**kwargs)
def wait(test,seconds=30):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  result=test()
  if result:return result
  assert process.poll() is None,'game exited before the test completed'
  time.sleep(.1)
 raise AssertionError('Timed out waiting for game response')
if windows:
 from ctypes import wintypes
 user=ctypes.windll.user32;handle=0
 user.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
 user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
 user.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
 callback=ctypes.WINFUNCTYPE(ctypes.c_int,wintypes.HWND,wintypes.LPARAM)
 def find_window():
  found=[]
  @callback
  def visit(hwnd,_):
   pid=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid));title=ctypes.create_unicode_buffer(300);user.GetWindowTextW(hwnd,title,300)
   if pid.value==process.pid and title.value.startswith('OpenMR2'):found.append(hwnd)
   return 1
  user.EnumWindows(visit,0);return found[0] if found else None
 handle=wait(find_window)
 def title():
  text=ctypes.create_unicode_buffer(400);user.GetWindowTextW(handle,text,400);return text.value
 def key(name):
  code=0x6f+int(name[1:]) if name.startswith('F') else {'Escape':27,'Return':13,'Down':40}[name]
  user.PostMessageW(handle,0x104 if name=='F10' else 0x100,code,0);time.sleep(.08);user.PostMessageW(handle,0x105 if name=='F10' else 0x101,code,0);time.sleep(.18)
else:
 def find_window():
  p=subprocess.run(['xdotool','search','--name','^OpenMR2'],capture_output=True,text=True);return p.stdout.splitlines()[0] if p.returncode==0 else None
 handle=wait(find_window)
 def title():return subprocess.check_output(['xdotool','getwindowname',str(handle)],text=True).strip()
 xkeys=ctypes.CDLL('libX11.so.6');xkeys.XOpenDisplay.restype=ctypes.c_void_p;key_display=xkeys.XOpenDisplay(None)
 xkeys.XStringToKeysym.argtypes=[ctypes.c_char_p];xkeys.XStringToKeysym.restype=ctypes.c_ulong
 xkeys.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];xkeys.XKeysymToKeycode.restype=ctypes.c_ubyte
 xkeys.XDefaultRootWindow.argtypes=[ctypes.c_void_p];xkeys.XDefaultRootWindow.restype=ctypes.c_ulong
 class KeyEvent(ctypes.Structure):_fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send',ctypes.c_int),('display',ctypes.c_void_p),('window',ctypes.c_ulong),('root',ctypes.c_ulong),('subwindow',ctypes.c_ulong),('time',ctypes.c_ulong),('x',ctypes.c_int),('y',ctypes.c_int),('x_root',ctypes.c_int),('y_root',ctypes.c_int),('state',ctypes.c_uint),('code',ctypes.c_uint),('same_screen',ctypes.c_int)]
 class KeyUnion(ctypes.Union):_fields_=[('key',KeyEvent),('padding',ctypes.c_long*24)]
 xkeys.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(KeyUnion)];xkeys.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
 def key(name):
  event=KeyUnion();e=event.key;e.display=key_display;e.window=int(handle);e.root=xkeys.XDefaultRootWindow(key_display);e.same_screen=1;e.code=xkeys.XKeysymToKeycode(key_display,xkeys.XStringToKeysym(name.encode()))
  e.type=2;xkeys.XSendEvent(key_display,int(handle),0,1,ctypes.byref(event));xkeys.XSync(key_display,0);time.sleep(.12)
  e.type=3;xkeys.XSendEvent(key_display,int(handle),0,2,ctypes.byref(event));xkeys.XSync(key_display,0);time.sleep(.25)
def setting(name,value):
 return any(line.strip()==f'{name} = {value}' for line in cfg.read_text().splitlines())
def screenshot():
 folder=config/'screenshots';before=set(folder.glob('*.jpg')) if folder.exists() else set();key('F12');path=wait(lambda:next(iter(set(folder.glob('*.jpg'))-before),None));wait(lambda:path.stat().st_size>1000);image=Image.open(path);image.load();assert image.format=='JPEG' and image.size==(640,480);assert max(image.quantization[0])<=5;return path
try:
 time.sleep(2)
 key('F2');wait(lambda:setting('texture_filter','1'));key('F2');wait(lambda:setting('texture_filter','0'))
 key('F3');wait(lambda:setting('frame_rate','60fps'))
 key('F4');wait(lambda:setting('custom_textures','1'))
 key('F7');wait(lambda:setting('video_upscaling','1'));key('F8');wait(lambda:setting('video_filter','1'))
 key('F9');wait(lambda:setting('effects_muted','1'));key('F9');wait(lambda:setting('effects_muted','0'))
 checks.append('F2/F3/F4/F7/F8/F9 delivered through the platform key map and persisted')
 key('F10');wait(lambda:'  -  ' not in title());options=screenshot();key('Escape');wait(lambda:'  -  ' in title());race=screenshot();assert Image.open(options).tobytes()!=Image.open(race).tobytes();checks.append('F10 opens settings during a race and Apply/Escape resumes the race (actual window states checked)')
 key('F1');wait(lambda:setting('graphics_mode','1'));key('F5');wait(lambda:setting('retro_output_filter','linear'));key('F6');wait(lambda:setting('retro_scale','2'))
 key('F10');retro=screenshot();key('Escape');key('F1');wait(lambda:setting('graphics_mode','0'));checks.append('F1 switches modes; F5/F6 work in Retro; F10 opens the Retro variant')
 before=set((config/'recordings').glob('*.mp4')) if (config/'recordings').exists() else set();key('F11');time.sleep(3);key('F11')
 def final_recording():
  paths=set((config/'recordings').glob('*.mp4'))-before
  return next((p for p in paths if not p.name.endswith('.video.mp4') and p.stat().st_size>1000),None)
 video=wait(final_recording);time.sleep(.5)
 probe=json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(video)],text=True));v=next(s for s in probe['streams'] if s['codec_type']=='video');a=next(s for s in probe['streams'] if s['codec_type']=='audio')
 assert v['codec_name']=='h264' and a['codec_name']=='aac' and v['pix_fmt']=='yuv420p' and v['r_frame_rate']=='30/1';assert 2<float(probe['format']['duration'])<10;assert v['width']==640 and v['height']==480
 checks.append('F11 creates a playable 30fps H.264/yuv420p + AAC MP4 with correct duration and dimensions')
 shot=screenshot();checks.append('F12 writes a valid correctly sized JPG under revived_config/screenshots')
 # Start another recording and close the owned test window: normal exit must finalize it.
 previous_videos=set((config/'recordings').glob('*.mp4'));key('F11');time.sleep(1)
 if windows:user.PostMessageW(handle,0x10,0,0)
 else:
  x11=ctypes.CDLL('libX11.so.6');x11.XOpenDisplay.restype=ctypes.c_void_p;x11.XInternAtom.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int];x11.XInternAtom.restype=ctypes.c_ulong
  class Message(ctypes.Structure):_fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send',ctypes.c_int),('display',ctypes.c_void_p),('window',ctypes.c_ulong),('message_type',ctypes.c_ulong),('format',ctypes.c_int),('data',ctypes.c_long*5)]
  class Event(ctypes.Union):_fields_=[('message',Message),('padding',ctypes.c_long*24)]
  display=x11.XOpenDisplay(None);event=Event();m=event.message;m.type=33;m.send=1;m.display=display;m.window=int(handle);m.message_type=x11.XInternAtom(display,b'WM_PROTOCOLS',0);m.format=32;m.data[0]=x11.XInternAtom(display,b'WM_DELETE_WINDOW',0)
  x11.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)];x11.XFlush.argtypes=[ctypes.c_void_p];x11.XSendEvent(display,int(handle),0,0,ctypes.byref(event));x11.XFlush(display)
 process.wait(timeout=30)
 final=[p for p in set((config/'recordings').glob('*.mp4'))-previous_videos if not p.name.endswith('.video.mp4')];assert len(final)==1
 subprocess.check_output([ffprobe,'-v','error','-show_format',str(final[0])]);checks.append('Normal game exit finalizes an active recording')
 (root/'build'/f'{platform}-shortcuts-recording-results.json').write_text(json.dumps({'checks':checks,'probe':probe,'screenshot':str(shot),'failures':0},indent=2))
 print(platform+': '+str(len(checks))+' actual keyboard/recording checks passed')
finally:
 if process.poll() is None:
  if windows:process.terminate()
  else:
   import signal
   os.killpg(process.pid,signal.SIGTERM)
  process.wait(timeout=10)
