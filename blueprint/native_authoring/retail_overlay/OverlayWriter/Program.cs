using UAssetAPI;
using UAssetAPI.UnrealTypes;
using UAssetAPI.Unversioned;

if (args.Length != 3) throw new ArgumentException("Expected input JSON, output asset, and mapping");
var mapping = new Usmap(args[2]);
var asset = UAsset.DeserializeJson(File.ReadAllText(args[0]));
asset.Mappings = mapping;
asset.SetEngineVersion(EngineVersion.VER_UE5_3);
asset.Write(args[1]);
var readback = new UAsset(args[1], EngineVersion.VER_UE5_3, mapping);
File.WriteAllText(args[1] + ".readback.json", readback.SerializeJson(false));
Console.WriteLine($"Wrote and decoded {args[1]} exports={readback.Exports.Count}");
