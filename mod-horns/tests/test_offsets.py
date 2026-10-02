"""Execute the actual Lua modules against controlled UE4SS fixtures using Lupa."""
from pathlib import Path
from tempfile import TemporaryDirectory
from lupa.lua54 import LuaRuntime

root = Path(__file__).resolve().parents[1]
lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().TEST_SCRIPTS = (root / "ue4ss/OBRHornsForAll/Scripts").as_posix()
with TemporaryDirectory(prefix="hfa-offset-test-") as temp:
    lua.globals().TEST_TEMP = Path(temp).as_posix()
    lua.execute((root / "tests/test_offsets.lua").read_text(encoding="utf-8"))
