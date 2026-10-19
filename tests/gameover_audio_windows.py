from pathlib import Path
import argparse,subprocess,time,json,ctypes,shutil,array,math,os,hashlib
from ctypes import wintypes
if os.name!='nt':raise SystemExit('This real Windows keyboard/recording check requires Windows.')
p=argparse.ArgumentParser(description='Record the real race-to-Game-Over audio transition and verify silence.')
for key in ['binary','game','receipt','ffmpeg','output']:p.add_argument('--'+key,type=Path,required=True)
args=p.parse_args();run=args.output.resolve();run.mkdir(parents=True,exist_ok=True)
exe=run/'OpenMR2.exe';shutil.copy2(args.binary,exe);(run/'tools').mkdir(exist_ok=True);shutil.copy2(args.ffmpeg,run/'tools/ffmpeg.exe')
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
receipt='\n'.join(l for l in args.receipt.read_text().splitlines() if l.startswith(('Files_OK =','verified_directory =','verified_language =')))
cfg.write_text(receipt+'\nremember_startup_mode = 1\nstartup_mode = 0\ngraphics_mode = 0\nmsaa = 0\nfxaa = 0\nframe_rate = 60fps\nsounds_volume = 100\nengines_volume = 100\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 100\n')
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW
process=subprocess.Popen([str(exe),'--game',str(args.game.resolve()),'--track','1','--frame','150','--objtest','--graphics-test','22','--size','640x480','--quit','30'],cwd=run,startupinfo=si)
user=ctypes.windll.user32;user.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM];user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)];user.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int];callback=ctypes.WINFUNCTYPE(ctypes.c_int,wintypes.HWND,wintypes.LPARAM)
def find():
 found=[]
 @callback
 def visit(hwnd,_):
  pid=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid));text=ctypes.create_unicode_buffer(400);user.GetWindowTextW(hwnd,text,400)
  if pid.value==process.pid and text.value.startswith('OpenMR2'):found.append(hwnd)
  return 1
 user.EnumWindows(visit,0);return found[0] if found else 0
end=time.monotonic()+30;handle=0
while not handle and time.monotonic()<end:handle=find();time.sleep(.1)
assert handle
rec=run/'revived_config/recordings';before=set(rec.glob('*.mp4')) if rec.exists() else set()
def key(code):user.PostMessageW(handle,0x100,code,0);time.sleep(.1);user.PostMessageW(handle,0x101,code,0)
try:
 time.sleep(.7);key(0x7a);time.sleep(8)
 text=ctypes.create_unicode_buffer(500);user.GetWindowTextW(handle,text,500);assert text.value.startswith('OpenMR2'),text.value
 key(0x7b);key(0x7a);time.sleep(2)
 paths=set(rec.glob('*.mp4'))-before;assert len(paths)==1,paths;video=paths.pop()
 pcm=subprocess.check_output([str(run/'tools/ffmpeg.exe'),'-v','error','-i',str(video),'-vn','-f','s16le','-acodec','pcm_s16le','pipe:1'])
 samples=array.array('h',pcm);first=samples[:22050*2];last=samples[-22050*2:]
 head=math.sqrt(sum(v*v for v in first)/len(first));tail=max(map(abs,last));assert head>50,head;assert tail==0,tail
 subprocess.run([str(run/'tools/ffmpeg.exe'),'-v','error','-sseof','-1','-i',str(video),'-frames:v','1','-y',str(run/'gameover.png')],check=True)
 from PIL import Image
 im=Image.open(run/'gameover.png').convert('RGB');orange=sum(1 for red,green,blue in im.crop((320,350,640,480)).getdata() if red>160 and 60<green<210 and blue<100);assert orange>150,orange
 result={'gameover_orange_pixels':orange,'video':video.name,'race_audio_rms':head,'gameover_last_second_peak':tail,'window_title':text.value,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'failures':0};(run/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
 user.PostMessageW(handle,0x10,0,0)
 try:process.wait(timeout=10)
 except subprocess.TimeoutExpired:process.terminate();process.wait()
