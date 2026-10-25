// i made this in may 2025, i think i used ai
// i was trying to import sc level files into unity
// just committing this in case i need it some day (chances of that are 0% tbh)

// Assets/Editor/LevelEditorWindow.cs
using UnityEngine;
using UnityEditor;
using System.IO;
using System.Collections.Generic;
using System.Linq;

public class LevelEditorWindow : EditorWindow
{
    private struct ElementInfo
    {
        public string File;       // relative .x file
        public float Scaling;
        public float Rotation;    // degrees around Y
    }

    private struct LevelObject
    {
        public string Name;
        public float MinX, MinY, MinZ, MaxX, MaxY, MaxZ;
        public byte Variant;
        public GameObject Go;   // the scene instance
    }

    private readonly List<LevelObject> _objects = new();
    private string _inputPath = "Assets/Levels/001.dat";
    private string _outputPath = "Assets/Levels/001-edited.dat";
    private string _elementsTxt = "Assets/Levels/elements.txt";

    // maps ELEMENT name -> ElementInfo
    private readonly Dictionary<string, ElementInfo> _elementMap = new();

    [MenuItem("Tools/Level Editor")]
    static void OpenWindow()
    {
        EditorWindow wnd = GetWindow<LevelEditorWindow>("Level Editor");
        wnd.minSize = new Vector2(500, 250);
    }

    void OnGUI()
    {
        // Input selection
        GUILayout.BeginHorizontal();
        _inputPath = EditorGUILayout.TextField("Input .dat", _inputPath);
        if (GUILayout.Button("Browse", options: GUILayout.Width(60)))
        {
            var path = EditorUtility.OpenFilePanel("Select Level File", "Assets/Levels", "dat");
            if (!string.IsNullOrEmpty(path))
            {
                _inputPath = path;
                _inputPath = _inputPath.Replace(Application.dataPath, "Assets");
            }
        }
        GUILayout.EndHorizontal();

        // Output selection
        GUILayout.BeginHorizontal();
        _outputPath = EditorGUILayout.TextField("Output .dat", _outputPath);
        if (GUILayout.Button("Browse", options: GUILayout.Width(60)))
        {
            var path = EditorUtility.OpenFilePanel("Select Level File", "Assets/Levels", "dat");
            if (!string.IsNullOrEmpty(path))
            {
                _outputPath = path;
                _outputPath = _outputPath.Replace(Application.dataPath, "Assets");
            }
        }
        GUILayout.EndHorizontal();

        // Elements selection
        GUILayout.BeginHorizontal();
        _elementsTxt = EditorGUILayout.TextField("elements.txt", _elementsTxt);
        if (GUILayout.Button("Browse", options: GUILayout.Width(60)))
        {
            var path = EditorUtility.OpenFilePanel("Select elements.txt File", "Assets/Levels", "txt");
            if (!string.IsNullOrEmpty(path))
            {
                _elementsTxt = path;
                _elementsTxt = _elementsTxt.Replace(Application.dataPath, "Assets");
            }
        }
        GUILayout.EndHorizontal();


        if (GUILayout.Button("Load Level"))
        {
            ParseElementsFile();
            LoadLevel();
        }
        if (_objects.Count > 0 && GUILayout.Button("Save Level"))
        {
            SaveLevel();
        }
    }

    void ParseElementsFile()
    {
        _elementMap.Clear();
        if (!File.Exists(_elementsTxt))
        {
            Debug.LogError($"Elements file not found: {_elementsTxt}");
            return;
        }
        var lines = File.ReadAllLines(_elementsTxt);
        var current = new ElementInfo();
        string currentName = null;
        // file name parsing and scaling / radius parsing seems to be broken
        // it expects the x files to be in a folder, some FILE also is .ani and not .x
        foreach (var raw in lines)
        {
            var line = raw.Trim();
            if (string.IsNullOrEmpty(line)) continue;
            if (line.StartsWith("ELEMENT"))
            {
                // Save previous
                if (currentName != null)
                    _elementMap[currentName] = current;
                // New element
                var q1 = line.IndexOf('"');
                var q2 = line.LastIndexOf('"');
                currentName = line.Substring(q1+1, q2-q1-1);
                current = new ElementInfo { File = null, Scaling = 1f, Rotation = 0f };
            }
            else if (currentName != null)
            {
                if (line.StartsWith("FILE"))
                {
                    var parts = line.Split('"');
                    current.File = parts[1].Replace("\\", "/");
                }
                else if (line.StartsWith("SCALING"))
                {
                    var v = line.Split('\t')[1];
                    if (float.TryParse(v, out var f)) current.Scaling = f;
                }
                else if (line.StartsWith("ROTATION"))
                {
                    var v = line.Split('\t')[1];
                    if (float.TryParse(v, out var f)) current.Rotation = f;
                }
            }
        }
        // last
        if (currentName != null)
            _elementMap[currentName] = current;

        Debug.Log($"Parsed {_elementMap.Count} elements from elements.txt");
    }

    void LoadLevel()
    {
        // cleanup old
        foreach (var obj in _objects.Where(obj => obj.Go)) DestroyImmediate(obj.Go);
        _objects.Clear();

        using (var fs = new FileStream(_inputPath, FileMode.Open, FileAccess.Read))
        using (var reader = new BinaryReader(fs))
        {
            var count = reader.ReadUInt32();
            for (var i = 0; i < count; i++)
            {
                var nameBuf = reader.ReadBytes(32);
                // this thing is shit cuz it has NULL and ????? at the end? which messes up logging
                var name = System.Text.Encoding.ASCII.GetString(nameBuf).TrimEnd('\0', (char)0xCD);
                var minX = reader.ReadSingle();
                var minY = reader.ReadSingle();
                var minZ = reader.ReadSingle();
                var maxX = reader.ReadSingle();
                var maxY = reader.ReadSingle();
                var maxZ = reader.ReadSingle();
                var variant = reader.ReadByte();
                reader.BaseStream.Seek(3, SeekOrigin.Current);

                // Determine prefab path from elementMap
                GameObject prefab = null;
                Debug.Log($"Loading  ({minX}, {minY}, {minZ}) ({maxX}, {maxY}, {maxZ}) and variant {variant}");

                // THIS PATH IS NEVER TAKEN, why?
                if (_elementMap.TryGetValue(name, out var info) && !string.IsNullOrEmpty(info.File))
                {
                    // convert gfx/xxx.x to Assets/Models/xxx.fbx
                    var baseName = Path.GetFileNameWithoutExtension(info.File);
                    var fbxPath = $"Assets/Models/{baseName}.fbx";
                    prefab = AssetDatabase.LoadAssetAtPath<GameObject>(fbxPath);
                    Debug.Log("Prefab path: " + fbxPath);
                }

                var go = PrefabOrCube(prefab);
                go.name = name;

                // position
                var center = new Vector3((minX + maxX)*.5f, (minY + maxY)*.5f, (minZ + maxZ)*.5f);
                go.transform.position = center;

                // apply scaling and box size
                var boxSize = new Vector3(Mathf.Abs(maxX-minX), Mathf.Abs(maxY-minY), Mathf.Abs(maxZ-minZ));
                var uniformScale = _elementMap.TryGetValue(name, out info) ? info.Scaling : 1f;
                go.transform.localScale = boxSize * uniformScale;

                // apply rotation around Y if provided
                if (_elementMap.TryGetValue(name, out info))
                    go.transform.rotation = Quaternion.Euler(0, info.Rotation, 0);

                _objects.Add(new LevelObject { Name=name, MinX=minX, MinY=minY, MinZ=minZ, MaxX=maxX, MaxY=maxY, MaxZ=maxZ, Variant=variant, Go=go });
            }
        }
        Debug.Log($"Loaded {_objects.Count} objects.");
    }

    void SaveLevel()
    {
        using (var fs = new FileStream(_outputPath, FileMode.Create, FileAccess.Write))
        using (var writer = new BinaryWriter(fs))
        {
            writer.Write((uint)_objects.Count);
            foreach (var obj in _objects)
            {
                var t = obj.Go.transform;
                var c = t.position;
                var half = t.localScale * .5f;
                float minX = c.x - half.x, minY = c.y - half.y, minZ = c.z - half.z;
                float maxX = c.x + half.x, maxY = c.y + half.y, maxZ = c.z + half.z;

                var nameBuf = new byte[32];
                var nb = System.Text.Encoding.ASCII.GetBytes(obj.Name);
                System.Array.Copy(nb, nameBuf, Mathf.Min(nb.Length, 31));
                writer.Write(nameBuf);

                writer.Write(minX); writer.Write(minY); writer.Write(minZ);
                writer.Write(maxX); writer.Write(maxY); writer.Write(maxZ);
                writer.Write(obj.Variant);
                writer.Write(new byte[3]);
            }
        }
        AssetDatabase.Refresh();
        Debug.Log($"Level saved to {_outputPath}");
    }

    GameObject PrefabOrCube(GameObject prefab)
    {
        if (prefab)
            return (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        var cube = GameObject.CreatePrimitive(PrimitiveType.Cube);
        cube.GetComponent<Renderer>().sharedMaterial =
            AssetDatabase.GetBuiltinExtraResource<Material>("Default-Material.mat");
        return cube;
    }
}
