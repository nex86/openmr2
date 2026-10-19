"""Combine public Markdown documents and keep their links usable at repository root."""
import posixpath
import re
from urllib.parse import unquote, urlsplit

MANUAL = 'DOCUMENTATION.md'
ORDER = ['CONFIGURATION.md', 'KEYBOARD.md', 'RECORDING.md', 'REVIVE_EFFECTS.md',
         'VIDEO_UPSCALING.md', 'PHYSICS.md', 'RETRO_RENDERER.md', 'BUILDING.md',
         'CONTRIBUTING.md', 'DISTRIBUTION.md', 'LICENSING.md', 'LICENSE_STATUS.md',
         'PUBLICATION_AUDIT.md', 'CHANGELOG.md', 'RELEASE_NOTES.md', 'TODO.md']
TITLES = {'RELEASE_NOTES.md': 'Release notes', 'BUILDING.md': 'Building from source',
          'src/third_party/README.md': 'Vendored PNG libraries'}


def section_id(name):
    return name.removesuffix('.md').lower().replace('/', '-').replace('_', '-')


def heading_id(title):
    return re.sub(r'[^\w\-\s]', '', title.lower()).replace(' ', '-')


def rewrite_links(text, source, names, merged=False):
    """Rebase nested image/file links and map former Markdown paths to anchors."""
    def replace(match):
        target = match.group(1)
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc:
            return match.group(0)
        path = unquote(parsed.path)
        if not path:
            if merged and parsed.fragment:
                return '](#'+section_id(source)+'--'+parsed.fragment+')'
            return match.group(0)
        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), path))
        if resolved in names and resolved != 'README.md':
            anchor = section_id(resolved)
            if parsed.fragment:
                anchor += '--'+parsed.fragment
            return ']('+('' if merged else MANUAL)+'#'+anchor+')'
        suffix = ('?'+parsed.query if parsed.query else '')+('#'+parsed.fragment if parsed.fragment else '')
        return ']('+resolved+suffix+')'
    # Fenced examples are examples, not Markdown links to be changed.
    output = []
    fence = None
    for line in text.splitlines(keepends=True):
        marker = re.match(r'^\s*(`{3,}|~{3,})', line)
        if marker:
            if fence is None:
                fence = marker.group(1)[0]
            elif fence == marker.group(1)[0]:
                fence = None
            output.append(line)
        elif fence:
            output.append(line)
        else:
            output.append(re.sub(r'\]\(([^\s)]+)\)', replace, line))
    return ''.join(output)


def consolidate_documents(documents):
    """Return only README.md and DOCUMENTATION.md; preserve all document text."""
    names = set(documents)-{'README.md', MANUAL}
    if not names:
        return {name: documents[name] for name in ('README.md', MANUAL) if name in documents}
    order = sorted(names, key=lambda n: (ORDER.index(n) if n in ORDER else len(ORDER), n))
    titles = {}
    for name in order:
        title = re.search(r'^# ([^\n]+)', documents[name], re.M)
        titles[name] = TITLES.get(name, title.group(1).strip() if title else name.removesuffix('.md').replace('_', ' ').title())
    parts = ['# OpenMR2 Documentation\n\nSettings, controls, technical notes, development history and licensing.\n\n## Contents\n\n']
    parts += ['- ['+titles[name]+'](#'+section_id(name)+')\n' for name in order]
    for name in order:
        body = rewrite_links(documents[name], name, names, merged=True)
        parts.append('\n---\n\n<a id="'+section_id(name)+'"></a>\n\n')
        parts.append('## '+titles[name]+'\n\n')
        primary = re.match(r'^# '+re.escape(titles[name])+r'\s*\n', body)
        skip_title = bool(primary)
        fence = None
        used = {}
        for line in body.splitlines(keepends=True):
            marker = re.match(r'^\s*(`{3,}|~{3,})', line)
            if marker:
                if fence is None:
                    fence = marker.group(1)[0]
                elif fence == marker.group(1)[0]:
                    fence = None
            heading = re.match(r'^(#{1,6}) (.+?)\s*\n?$', line) if fence is None else None
            if heading:
                slug = heading_id(heading.group(2))
                count = used.get(slug, 0)
                used[slug] = count+1
                if count:
                    slug += '-'+str(count)
                parts.append('<a id="'+section_id(name)+'--'+slug+'"></a>\n\n')
                if skip_title:
                    skip_title = False
                else:
                    parts.append('#'*min(len(heading.group(1))+(1 if primary else 2), 6)+' '+heading.group(2)+'\n')
            else:
                parts.append(line)
        parts.append('\n')
    return {'README.md': rewrite_links(documents['README.md'], 'README.md', names),
            MANUAL: ''.join(parts)}
