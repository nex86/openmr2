"""Flag likely German UI literals not covered by the translation table; manual review aid."""
import csv,json,pathlib,re
root=pathlib.Path(__file__).resolve().parents[1]
known={(r.get('key') or r['de']).replace('\\n','\n') for r in csv.DictReader((root/'localization.tsv').open(encoding='utf-8'),delimiter='\t')}
token=r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*'
markers=re.compile(r'[äöüßÄÖÜ]|\b(?:Wagen|Rennen|Runde|Runden|Strecke|Geld|Schaden|Taste|Tasten|Hilfe|Beenden|Modell|Modelle|Fahrzeugsatz|Dreiecke|Raeder|Fahrbahn|Lenk|Punkt|Bild|Ausgabe|Zurueck|Abbrechen|Schutzschild|Oel|Werfer|Hoch|Niedrig|Mittel|Verfolgung|Kapazitaet|Munition|Sprengstoff|Kraftstoff|Fluessiges|Fluessiger|Eistee|Ton)\b')
missing={}
for name in ('g_menu_mid.inc','g_menu_tail.inc','g_race.inc','g_track.inc','modes.inc','core.cpp','win32_main.cpp'):
    t=(root/'src'/name).read_text(encoding='utf-8')
    # Keep literals intact while discarding comments.
    items=re.finditer(token+r'|//[^\n]*|/\*.*?\*/',t,re.S)
    values=[]
    for m in items:
        if not m.group().startswith('"'):continue
        value=''.join(bytes(s,'utf8').decode('unicode_escape') for s in re.findall(r'"((?:[^"\\]|\\.)*)"',m.group()))
        if '|  ÀÉÈÊ' in value:continue  # Intentional multilingual font sample.
        if value not in known and markers.search(value) and not re.fullmatch('[a-z_0-9]+',value):values.append(value)
    if values:missing[name]=list(dict.fromkeys(values))
print(json.dumps(missing,ensure_ascii=True,indent=2))
