"""Use the cached UBT response files to compile an isolated authoring module."""
from pathlib import Path
source=Path(__file__).with_name('compile_skin_scratch.py').read_text()
source=source.replace('compile-skin-20261001','compile-skeleton-preview-20261001')
source=source.replace("Path('D:/Unblivion Editor/Manifests/PerfEditor.lock')","repo / '.work/extended-races-bp-authoring/skeleton-mesh-prototype/editor-validation.lock'")
exec(compile(source,__file__,'exec'))
