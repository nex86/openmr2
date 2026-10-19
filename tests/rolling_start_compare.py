from pathlib import Path
import argparse,csv,json,math
p=argparse.ArgumentParser(description='Compare real rolling-start trajectories with a saved stable reference.')
p.add_argument('--reference',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
x=list(csv.DictReader(a.reference.open()));y=list(csv.DictReader(a.candidate.open()))
assert len(x)==len(y)==3600,'Expected six tracks, eight vehicles and 75 updates per track'
fields=['autopilot','node','x','heading','steer','lateral','speed','damage']
bad=[{'row':i,'field':f,'reference':r[f],'candidate':s[f]} for i,(r,s) in enumerate(zip(x,y)) for f in fields if abs(float(r[f])-float(s[f]))>1e-6]
report={'tracks':6,'cars':8,'updates_per_track':75,'rows':len(y),'differences':len(bad),'first_differences':bad[:5]}
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(bool(bad))
