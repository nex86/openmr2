"""One-time migration of literal UI fragments; never translates runtime user data."""
import csv
import pathlib
import re

root=pathlib.Path(__file__).resolve().parents[1]
translations={(r.get('key') or r['de']).replace('\\n','\n') for r in csv.DictReader((root/'localization.tsv').open(encoding='utf-8'),delimiter='\t')}
token=r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*'
def decoded(s):
    return ''.join(bytes(q,'utf8').decode('unicode_escape') for q in re.findall(r'"((?:[^"\\]|\\.)*)"',s))
for name in ['g_menu_tail.inc','g_menu_mid.inc','g_race.inc','g_track.inc','modes.inc','core.cpp']:
    path=root/'src'/name
    text=path.read_text(encoding='utf-8')
    def wrap(m):
        prefix,suffix=m.group(1),m.group(2)
        return prefix+('ui_translate('+suffix+')' if decoded(suffix) in translations else suffix)
    text=re.sub(r'((?:str_cat|str_cpy)\([^,\n]+,\s*)('+token+')',wrap,text)
    def init(m):
        name,cap,s=m.groups()
        if decoded(s) not in translations:return m.group()
        return 'char '+name+'['+cap+'] = ""; str_cpy('+name+', ui_translate('+s+'), '+cap+');'
    text=re.sub(r'char (\w+)\[(\d+)\]\s*=\s*('+token+');',init,text)
    # These tables contain program-owned UI data, never player names or user paths.
    text=re.sub(r'((?:str_cat|str_cpy)\([^,\n]+,\s*)((?:TRACKS|CARS|WEAPONS)\[[^\]]+\]\.(?:name|fuel|launcher|explosive|dmgtxt))(?=\s*,)',lambda m:m.group(1)+'ui_translate('+m.group(2)+')',text)
    path.write_text(text,encoding='utf-8')
