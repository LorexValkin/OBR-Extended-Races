"""Verify that extending the voice maps preserves every retail response field."""
from pathlib import Path
import json
import sys

sys.path.insert(0, 'D:/Dump Experiment/FullDump/src')
import query_cache as cache
root = Path(sys.argv[1])
manifest = json.loads((root / 'voice-overlay-manifest.json').read_text())
db = cache.database()
checked = 0
try:
    for item in manifest['packages']:
        document = dict(db.execute(f'SELECT {cache.DOCUMENT_FIELDS} FROM documents WHERE id=?', (item['record'],)).fetchone())
        if document['sha256'] != item['captureSha256']:
            raise RuntimeError('Source capture identity changed')
        original = cache.payload(document)
        after = json.loads((root / (Path(item['owner']).stem + '.readback.json')).read_text())
        added = 0
        for before_export, after_export in zip(original['Exports'], after['Exports'], strict=True):
            if not isinstance(before_export.get('Data'), list):
                if before_export != after_export:
                    raise RuntimeError('Non-property export changed')
                continue
            before_data = before_export['Data']
            after_data = after_export['Data']
            for before_prop, after_prop in zip(before_data, after_data, strict=True):
                if before_prop['Name'] == 'Responses':
                    for before_response, after_response in zip(before_prop['Value'], after_prop['Value'], strict=True):
                        for before_field, after_field in zip(before_response['Value'], after_response['Value'], strict=True):
                            if before_field['Name'] in ('AkAudioEvents', 'Animations'):
                                old_entries = before_field['Value']
                                new_entries = after_field['Value']
                                if new_entries[:len(old_entries)] != old_entries:
                                    raise RuntimeError(f'Retail voice mapping was changed: {item["owner"]}')
                                added += len(new_entries) - len(old_entries)
                                after_field['Value'] = new_entries[:len(old_entries)]
            if after_data != before_data:
                raise RuntimeError(f'Retail response property changed: {item["owner"]}')
        if added != item['addedKeys']:
            raise RuntimeError('Added key count differs from receipt')
        checked += 1
finally:
    db.close()
receipt = {'packagesChecked': checked, 'retailResponseProperties': 'preserved',
           'retailAudioAndAnimationKeys': 'preserved', 'sourceCaptures': 'hash-authenticated',
           'retailExecution': 'not tested'}
(root / 'retail-preservation-verification.json').write_text(json.dumps(receipt, indent=2))
print(json.dumps(receipt, indent=2))
