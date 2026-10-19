"""Build a local Linux candidate, bundle non-driver dependencies and license notices."""
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess

root=pathlib.Path(__file__).resolve().parents[1]
build=root/'build'
app=build/'OpenMR2.AppDir'
(app/'usr/bin').mkdir(parents=True,exist_ok=True)
(app/'usr/lib').mkdir(parents=True,exist_ok=True)
licenses=app/'usr/share/doc/OpenMR2/third-party'
licenses.mkdir(parents=True,exist_ok=True)
runtime_license=build/'tools/AppImage-runtime-LICENSE'
if not runtime_license.exists():raise RuntimeError('Fetch the official runtime license before packaging')
shutil.copy2(runtime_license,licenses/'AppImage-runtime-LICENSE')
shutil.copy2(build/'tools/downloads.json',licenses/'AppImage-build-tools.json')
manual_docs = ('LICENSE','DOCUMENTATION.md','custom_textures.txt','THIRD_PARTY_NOTICES.txt','README.md')
legacy_docs = ('LICENSE','LICENSING.md','PUBLICATION_AUDIT.md','KEYBOARD.md','DISTRIBUTION.md','RECORDING.md','VIDEO_UPSCALING.md','TODO.md','REVIVE_EFFECTS.md','ENGINE_AUDIO.md','custom_textures.txt','THIRD_PARTY_NOTICES.txt','README.md','BUILDING.md','CONFIGURATION.md','LOCALIZATION_REVIEW.md','RETRO_RENDERER.md','CHANGELOG.md','HUD_RESEARCH.md','LICENSE_STATUS.md','RELEASE_NOTES.md')
for doc in manual_docs if (root/'DOCUMENTATION.md').is_file() else legacy_docs:
    shutil.copy2(root/doc,licenses.parent/doc)
# Private QA reports are not distributed in the public source snapshot.
# Their absence must not prevent building an optional AppImage from a clone.
for doc in ('TEST_REPORT.md','WORK_STATUS.md','test-results.json'):
    if (root/doc).is_file():shutil.copy2(root/doc,licenses.parent/doc)
# Bundle the complete filter sources, build adapter and license texts.
filter_sources=licenses.parent/'output-filter-source'
filter_sources.mkdir(exist_ok=True)
shutil.copytree(root/'src/third_party/output_filters',filter_sources/'upstream',dirs_exist_ok=True)
for file in ('src/output_filters.inc','src/output_filter_data.h','tools/build_output_filters.py'):
    shutil.copy2(root/file,filter_sources/pathlib.Path(file).name)
shutil.copytree(root/'assets/effects',licenses.parent/'effect-assets',dirs_exist_ok=True)
shutil.copytree(root/'assets/revived_ui',licenses.parent/'revived-ui-assets',dirs_exist_ok=True)
for name in ('fsr1','video_filters','fsrcnnx'):
    shutil.copytree(root/'src/third_party'/name,licenses.parent/('video-source-'+name),dirs_exist_ok=True)
for name in ('build_video_fsr.py','build_video_filters.py','build_video_cnn.py'):
    shutil.copy2(root/'tools'/name,licenses.parent/name)
shutil.copy2(root/'src/video_upscale.inc',licenses.parent/'video_upscale.inc')
for name in ('video_cnn.inc','video_cnn_data.h'):
    shutil.copy2(root/'src'/name,licenses.parent/name)
shutil.copy2(build/'openmr2_linux',app/'usr/bin/openmr2')
(app/'openmr2.desktop').write_text('[Desktop Entry]\nType=Application\nName=OpenMR2\nExec=openmr2\nIcon=openmr2\nCategories=Game;\nTerminal=false\n')
(app/'openmr2.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256"><rect width="256" height="256" rx="32" fill="#121820"/><path d="M32 176L80 72h96l48 104" fill="none" stroke="#d63030" stroke-width="16"/><text x="128" y="150" text-anchor="middle" fill="white" font-family="sans-serif" font-size="32">OpenMR2</text></svg>')
(app/'AppRun').write_text('''#!/bin/sh
set -eu
APPDIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export LD_LIBRARY_PATH="$APPDIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -n "${APPIMAGE:-}" ]; then search_dir="$(dirname -- "$APPIMAGE")"; else search_dir="$(dirname -- "$APPDIR")"; fi
data_dir="${XDG_DATA_HOME:-$HOME/.local/share}/OpenMR2"
mkdir -p "$data_dir"
cd "$data_dir"
exec "$APPDIR/usr/bin/openmr2" --program-directory "$search_dir" "$@"
''')
(app/'AppRun').chmod(0o755)
(app/'usr/bin/openmr2').chmod(0o755)
exclude={'libc.so.6','libm.so.6','libpthread.so.0','libdl.so.2','librt.so.1','ld-linux-x86-64.so.2'}
queue=[build/'openmr2_linux']
for lib in ('libGL.so.1','libpulse-simple.so.0'):
    paths=list(pathlib.Path('/usr/lib/x86_64-linux-gnu').glob(lib))
    if paths:queue+=paths
copied=set();records=[]
while queue:
    binary=queue.pop()
    output=subprocess.check_output(['ldd',str(binary)],text=True)
    for name,path in re.findall(r'^\s*(\S+) => (/\S+)',output,re.M):
        if name in exclude or name in copied:continue
        copied.add(name);p=pathlib.Path(path)
        shutil.copy2(p,app/'usr/lib'/name)
        queue.append(p)
        result=subprocess.run(['dpkg-query','-S',str(p.resolve())],text=True,capture_output=True)
        if result.returncode:
            result=subprocess.run(['dpkg-query','-S','*/'+p.resolve().name],text=True,capture_output=True)
        package=result.stdout.split(': ',1)[0].strip().split('\n')[0] if result.stdout else 'unknown'
        copyright=pathlib.Path('/usr/share/doc')/package.split(':')[0]/'copyright'
        if copyright.exists():shutil.copy2(copyright,licenses/(package.replace(':','_')+'-copyright'))
        metadata=subprocess.run(['dpkg-query','-W','-f=${Package}\t${Version}\t${source:Package}\t${source:Version}',package],capture_output=True,text=True).stdout
        records.append({'library':name,'package':package,'metadata':metadata,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'copyright_included':copyright.exists()})
    # Seed libraries are loaded via dlopen and must themselves be bundled.
    if binary.name in ('libGL.so.1','libpulse-simple.so.0') and binary.name not in copied:
        copied.add(binary.name);shutil.copy2(binary,app/'usr/lib'/binary.name)
        result=subprocess.run(['dpkg-query','-S','*/'+binary.resolve().name],capture_output=True,text=True)
        package=result.stdout.split(': ',1)[0].strip().split('\n')[0] if result.stdout else 'unknown'
        copyright=pathlib.Path('/usr/share/doc')/package.split(':')[0]/'copyright'
        if copyright.exists():shutil.copy2(copyright,licenses/(package.replace(':','_')+'-copyright'))
        records.append({'library':binary.name,'package':package,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'copyright_included':copyright.exists()})
common=pathlib.Path('/usr/share/common-licenses')
for p in common.iterdir():
    if p.is_file():shutil.copy2(p,licenses/('common-'+p.name))
(licenses/'libraries.json').write_text(json.dumps(records,indent=2))
(licenses/'README.txt').write_text('Local development candidate. Copyright notices accompany bundled system libraries.\nBefore public distribution, verify every license and the exact corresponding-source requirements.\nThe port source licensing/provenance is unresolved; this candidate is NOT cleared for public release.\nGPU drivers, the display server, the audio server and glibc are supplied by the host system.\n')
runtime=build/'tools/runtime-x86_64'
tool=build/'tools/appimagetool-x86_64.AppImage'
tool.chmod(0o755)
version=re.search(r'#define OPENMR2_VERSION "([^"]+)"',(root/'src/sys.h').read_text()).group(1)
env=dict(os.environ,ARCH='x86_64',APPIMAGE_EXTRACT_AND_RUN='1',VERSION=version)
output=build/f'OpenMR2-{version}-x86_64.AppImage'
subprocess.run([str(tool),'--runtime-file',str(runtime),str(app),str(output)],env=env,check=True)
output.chmod(0o755)
print(output.name,hashlib.sha256(output.read_bytes()).hexdigest())
