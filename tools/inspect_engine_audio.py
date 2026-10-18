"""Verify engine assignments against the original LE executable and WAVs, read only.

Usage: python tools/inspect_engine_audio.py [directory containing the three GOG fixtures]
Only metadata/checksums are written; no game assets are exported.
"""
from pathlib import Path
import hashlib, json, struct, sys, wave
from inspect_executable import le_image
root=Path(__file__).resolve().parents[1]
fixtures=Path(sys.argv[1]) if len(sys.argv)>1 else root.parents[1]
expected=[0,1,2,3,1,2,3,1,2,3,1,2,3,1]
addresses=[(0xdcfac,0x858b6),(0xdd63c,0x85f42),(0xdd64c,0x85f5a)]
results=[]
for edition,(table,pitch) in zip(('English','French','German'),addresses):
    game=fixtures/('Megarace 2 '+edition)
    memory,_=le_image(game/'M.EXE')
    assignments=list(struct.unpack_from('<14I',memory,table));assert assignments==expected
    model_start=memory.index(b'caisse30.m3D\0caisse90.m3D\0crusader.m3D\0')
    models=memory[model_start:].split(b'\0',14)[:14]
    sounds=[]
    for i in range(4):
        start=table+56+i*36
        name=memory[start:start+32].split(b'\0')[0].decode('ascii').replace('\\','/').upper()
        gain=struct.unpack_from('<I',memory,start+32)[0]
        assert name==['SFX/TURBINE.WAV','SFX/M_2.WAV','SFX/MOT3.WAV','SFX/M_2.WAV'][i]
        assert gain==[20,20,18,18][i]
        audio=game/name
        with wave.open(str(audio)) as wav:parameters=list(wav.getparams())
        sounds.append({'profile':i,'file':name,'gain':gain,'sha256':hashlib.sha256(audio.read_bytes()).hexdigest(),'wave_parameters':parameters})
    curve=memory[pitch:pitch+1400*4]
    results.append({'edition':edition,'assignment_table':hex(table),'pitch_table':hex(pitch),'pitch_sha256':hashlib.sha256(curve).hexdigest(),
                    'sounds':sounds,'cars':[{'model':m.decode('ascii'),'profile':p,'sound':sounds[p]['file'],'gain':sounds[p]['gain']} for m,p in zip(models,assignments)]})
assert len({v['pitch_sha256'] for v in results})==1
assert all(results[0]['sounds']==v['sounds'] for v in results)
dest=root/'build/engine-audio-research.json';dest.parent.mkdir(exist_ok=True)
dest.write_text(json.dumps(results,indent=2),encoding='utf-8')
print('PASS: all 42 car assignments, 12 sound profiles and all 4,200 pitch entries match across the original editions.')
