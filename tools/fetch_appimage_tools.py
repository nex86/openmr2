"""Download build tools from the official AppImage repositories, record exact digests."""
import hashlib
import base64
import json
import pathlib
import urllib.request

root = pathlib.Path(__file__).resolve().parents[1] / 'build/tools'
root.mkdir(parents=True, exist_ok=True)
records=[]
for repo, name in [('appimagetool','appimagetool-x86_64.AppImage'),('type2-runtime','runtime-x86_64')]:
    url=f'https://api.github.com/repos/AppImage/{repo}/releases/tags/continuous'
    request=urllib.request.Request(url,headers={'User-Agent':'OpenMR2-build'})
    release=json.load(urllib.request.urlopen(request))
    asset=next(a for a in release['assets'] if a['name']==name)
    target=root/name
    with urllib.request.urlopen(asset['browser_download_url']) as response:
        target.write_bytes(response.read())
    digest=hashlib.sha256(target.read_bytes()).hexdigest()
    expected=asset.get('digest')
    if expected and expected!='sha256:'+digest:raise RuntimeError('Download digest mismatch: '+name)
    records.append({'repository':repo,'tag':release['tag_name'],'published':release['published_at'],'asset':name,'sha256':digest,'upstream_digest':expected,'url':asset['browser_download_url']})
    print(name,digest)
(root/'downloads.json').write_text(json.dumps(records,indent=2))
request=urllib.request.Request('https://api.github.com/repos/AppImage/type2-runtime/license',headers={'User-Agent':'OpenMR2-build'})
license=json.load(urllib.request.urlopen(request))
(root/'AppImage-runtime-LICENSE').write_bytes(base64.b64decode(license['content']))
print('Runtime license:',license['license']['spdx_id'])
