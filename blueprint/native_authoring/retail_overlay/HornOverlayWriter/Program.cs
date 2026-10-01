using UAssetAPI;
using UAssetAPI.UnrealTypes;
using UAssetAPI.Unversioned;
using UAssetAPI.ExportTypes;
using UAssetAPI.Kismet.Bytecode.Expressions;
var mapping = new Usmap(args[2]);
var asset = UAsset.DeserializeJson(File.ReadAllText(args[0]));
asset.Mappings = mapping;
asset.SetEngineVersion(EngineVersion.VER_UE5_3);
var function = asset.Exports.OfType<FunctionExport>().Single(x => x.ObjectName.ToString() == "OnPropertySelected");
if (function.ScriptBytecode.Length != 5) throw new Exception("Unexpected patched statement count");
uint offset = 0;
foreach (var expression in function.ScriptBytecode) {
    expression.Visit(asset, ref offset, (child, position) => {
        if (child is EX_Context context) context.Offset = context.ContextExpression.GetSize(asset);
    });
}
var jump = (EX_JumpIfNot)function.ScriptBytecode[1];
jump.CodeOffset = function.ScriptBytecode.Take(3).Aggregate(0u, (n,e)=>n+e.GetSize(asset));
function.ScriptBytecodeSize = (int)offset;
asset.Write(args[1]);
var readback = new UAsset(args[1], EngineVersion.VER_UE5_3, mapping);
File.WriteAllText(args[1]+".readback.json",readback.SerializeJson(false));
Console.WriteLine($"Horn Blueprint bytes={offset} jump={jump.CodeOffset} exports={asset.Exports.Count}");

