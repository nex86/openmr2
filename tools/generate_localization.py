"""Compile UTF-8 translations to the original renderer's single-byte text encoding."""
import csv
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
rows = list(csv.DictReader((root/'localization.tsv').open(encoding='utf-8'), delimiter='\t'))
def literal(s):
    s = s.replace('\\n','\n')
    return '"'+''.join('\\x%02x'%b for b in s.encode('latin1'))+'"'
out=['// Generated from localization.tsv. Edit that UTF-8 file and regenerate.', '#pragma once', '#include "sys.h"',
     'struct Translation { const char *key, *de, *en, *fr; };', 'static const Translation TRANSLATIONS[] = {']
for r in rows:
    out.append('  {'+', '.join(literal(s) for s in (r.get('key') or r['de'],r['de'],r['en'],r['fr']))+'},')
out += ['};', 'static const char *ui_translate(const char *s) {', '  int language=sys_game_language();',
        '  for(unsigned i=0;i<sizeof TRANSLATIONS/sizeof TRANSLATIONS[0];i++){const char *a=s,*b=TRANSLATIONS[i].key;while(*a && *a==*b){a++;b++;}if(!*a && !*b)return language==2?TRANSLATIONS[i].de:language==1?TRANSLATIONS[i].fr:TRANSLATIONS[i].en;}',
        '  return s;', '}', '']
out += ['static const char *ui_debug_text(const char *s) {',
 '  for(unsigned i=0;i<sizeof TRANSLATIONS/sizeof TRANSLATIONS[0];i++){const char *a=s,*b=TRANSLATIONS[i].key;while(*a && *a==*b){a++;b++;}if(!*a && !*b)return TRANSLATIONS[i].en;}',
 '  return s;', '}', '']
(root/'src/i18n.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated', len(rows), 'translation entries')
