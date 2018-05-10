using UnityEngine;
using UnityEditor;
using System;
using System.Runtime.InteropServices;
using System.IO;
using System.Collections.Generic;
using UnityEngine.Assertions;

namespace VenusRendering
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]

    public class OpenFileName
    {
        public int structSize = 0;
        public IntPtr dlgOwner = IntPtr.Zero;
        public IntPtr instance = IntPtr.Zero;
        public String filter = null;
        public String customFilter = null;
        public int maxCustFilter = 0;
        public int filterIndex = 0;
        public String file = null;
        public int maxFile = 0;
        public String fileTitle = null;
        public int maxFileTitle = 0;
        public String initialDir = null;
        public String title = null;
        public int flags = 0;
        public short fileOffset = 0;
        public short fileExtension = 0;
        public String defExt = null;
        public IntPtr custData = IntPtr.Zero;
        public IntPtr hook = IntPtr.Zero;
        public String templateName = null;
        public IntPtr reservedPtr = IntPtr.Zero;
        public int reservedInt = 0;
        public int flagsEx = 0;
    }
    public class DllHelper
    {
        [DllImport("Comdlg32.dll", SetLastError = true, ThrowOnUnmappableChar = true, CharSet = CharSet.Auto)]
        public static extern bool GetOpenFileName([In, Out] OpenFileName ofn);
        [DllImport("Comdlg32.dll", SetLastError = true, ThrowOnUnmappableChar = true, CharSet = CharSet.Auto)]
        public static extern bool GetSaveFileName([In, Out] OpenFileName ofn);
    }

    class LevelImporter
    {
        static readonly string sResPath = "Assets/Levels/";
        static Dictionary<string, Texture2D> mTextureMap = new Dictionary<string, Texture2D>();
        static List<GameObject> mOldObj = new List<GameObject>();
        static Transform mRootObj = null;
        static Transform mGroundObj = null;
        static Transform mEffectObj = null;
        static List<string> mFinishList = new List<string>();

        static float mUseTime = 0.0f;

        enum SupportedMaterialType
        {
            MAT_SCENE_GRASS,
            MAT_SCENE_PLAIN,
            MAT_SCENE_PLAIN_ALPHA,
            MAT_SCENE_PBR,
            MAT_SCENE_PBR_ALPHA,
            MAT_SCENE_PBR_GLOW,
            MAT_TERRAIN_PBR,
            MAT_WATER,
            MAT_MAX
        };

        enum TextureType
        {
            TEX_BASE,
            TEX_GLOW,
            TEX_NORMAL,
            TEX_MIX,
            TEX_LIGHTMAP
        }

        struct FogInfo
        {
            Vector3 position;
            float fogDensity;
            Vector4 fogInscatteringColor;
            float fogHeightFalloff;
            float fogMaxOpacity;
            float startDistance;
            float fogCutoffDistance;
        }

        struct ReflectionProbeInfo
        {
            public string name;
            public Vector3 position;
            public Vector3 offset;
            public float brightness;
            public float radius;
            public float averageBrightness;
        }

        struct LayerType
        {
            public int layers;
            public int vertexCopy;
        }

        class VertexLayer
        {
            public VertexLayer()
            {
                data = ~0;
            }

            public int Get(int idx)
            {
                if (idx < 0 || idx >= 4) idx = 0;
                return ((data >> (idx * 8)) & 0xFF);
            }

            public void Set(int idx, int layer)
            {
                if (idx < 0 || idx >= 4) idx = 0;
                data = (data & (~(0xFF << (idx * 8)))) | ((layer & 0xFF) << (idx * 8));
            }

            public int Find(int layer)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (Get(i) == layer)
                    {
                        return i;
                    }
                }
                return -1;
            }

            public bool Fit(LayerType type)
            {
                List<int> layers = new List<int>();
                for (int i = 0; i < 16; ++i)
                {
                    if ((type.layers & (1 << i)) != 0)
                    {
                        layers.Add(i);
                    }
                }
                Assert.IsTrue(layers.Count < 4);
                int rollBack = data;
                foreach (int layer in layers)
                {
                    if (Find(layer) < 0)
                    {
                        int emp = Find(0xFF);
                        if (emp < 0)
                        {
                            data = rollBack;
                            return false;
                        }
                        Set(emp, layer);
                    }
                }

                return true;
            }

            int data;
        }

        struct VertexInfo
        {
            public int copy;
            public int copyNum;
            public int copyStart;
        }

        struct LandscapeCell
        {
            public int x, y;
            public string lightMap;
            public Vector4 uvScaleBias;
            public Vector4 scale0;
            public Vector4 add0;
            public Vector4 scale1;
            public Vector4 add1;
            public Vector4 penumbra;
            public Color32[] heightNormalMap;
            public int layerNumber;
            public int[] layerIndexes;
            public byte[] weightMaps;

            public void Build(int quadPointNum)
            {
                layerPointMap = new int[heightNormalMap.Length];
                for (int i = 0; i < layerPointMap.Length; ++i)
                {
                    layerPointMap[i] = 0;
                    for (int j = 0; j < layerIndexes.Length; ++j)
                    {
                        byte weight = weightMaps[i * layerIndexes.Length + j];
                        if (weight != 0)
                        {
                            layerPointMap[i] |= 1 << j;
                        }
                    }
                }
                int quadNum = quadPointNum - 1;
                layerQuadMap = new int[quadNum * quadNum];
                HashSet<int> quadLayerType = new HashSet<int>();
                for (int i = 0; i < quadNum; ++i)
                {
                    for (int j = 0; j < quadNum; ++j)
                    {
                        layerQuadMap[i * quadNum + j] = layerPointMap[i * quadPointNum + j]
                            | layerPointMap[i * quadPointNum + j + 1]
                            | layerPointMap[(i + 1) * quadPointNum + j]
                            | layerPointMap[(i + 1) * quadPointNum + j + 1];

                        quadLayerType.Add(layerQuadMap[i * quadNum + j]);
                        
                    }
                }

                Dictionary<int, int> layerTypeDict = new Dictionary<int, int>();
                layerTypeArray = new LayerType[quadLayerType.Count];
                {
                    int i = 0;
                    foreach (var type in quadLayerType)
                    {
                        layerTypeArray[i].layers = type;
                        layerTypeArray[i].vertexCopy = -1;
                        layerTypeDict.Add(type, i);
                        ++i;
                    }
                }
                
                List<VertexLayer> vertexLayerList = new List<VertexLayer>();
                for (int i = 0; i < layerTypeArray.Length; ++i)
                {
                    for (int j = 0; j < vertexLayerList.Count; ++j)
                    {
                        if (vertexLayerList[j].Fit(layerTypeArray[i]))
                        {
                            layerTypeArray[i].vertexCopy = j;
                            break;
                        }
                    }
                    if (layerTypeArray[i].vertexCopy < 0)
                    {
                        VertexLayer newLayer = new VertexLayer();
                        newLayer.Fit(layerTypeArray[i]);
                        layerTypeArray[i].vertexCopy = vertexLayerList.Count;
                        vertexLayerList.Add(newLayer);
                    }
                }

                vertexLayerArray = vertexLayerList.ToArray();

                vertexInfoArray = new VertexInfo[quadPointNum * quadPointNum];
                for (int i = 0; i < quadPointNum; ++i)
                {
                    for (int j = 0; j < quadPointNum; ++j)
                    {
                        vertexInfoArray[i * quadPointNum + j].copy = 0;
                        vertexInfoArray[i * quadPointNum + j].copyStart = -1;

                        for (int k = 0; k < vertexLayerArray.Length; ++k)
                        {
                            bool used = false;
                            for (int l = -1; l < 1; ++l)
                            {
                                for (int m = -1; m < 1; ++m)
                                {
                                    int _l = i + l;
                                    int _m = j + m;
                                    if (_l >= 0 && _l < quadNum && _m >= 0 && _m < quadNum)
                                    {
                                        int typeIndex = layerTypeDict[layerQuadMap[_l * quadNum + _m]];
                                        if (layerTypeArray[typeIndex].vertexCopy == k)
                                        {
                                            used = true;
                                        }
                                    }
                                }
                            }
                            if (used)
                            {
                                vertexInfoArray[i * quadPointNum + j].copy |= 1 << k;
                            }

                        }
                        Assert.IsTrue(vertexInfoArray[i * quadPointNum + j].copy != 0);
                    }
                }

                int pointer = quadPointNum * quadPointNum;
                for (int i = 0; i < vertexInfoArray.Length; ++i)
                {
                    vertexInfoArray[i].copyStart = pointer;
                    int num = 0;
                    for (int j = 0; j < vertexLayerArray.Length; ++j)
                    {
                        if ((vertexInfoArray[i].copy & (1 << j)) != 0)
                        {
                            ++num;
                        }
                    }
                    vertexInfoArray[i].copyNum = num - 1;
                    pointer += vertexInfoArray[i].copyNum;
                }
                expandedVertexNum = pointer - quadPointNum * quadPointNum;
            }

            public int[] layerPointMap;
            public int[] layerQuadMap;
            public LayerType[] layerTypeArray;
            public VertexLayer[] vertexLayerArray;
            public VertexInfo[] vertexInfoArray;
            public int expandedVertexNum;

        }

        struct LayerInfo
        {
            public float tiling;
            public float yaw;
            public float pitch;
            public float specular;
            public string baseMap;
            public string normalMap;
        }

        struct LandscapeInfo
        {
            public string name;
            public Vector3 location;
            public Vector3 scale;
            public Vector4 lightMapScaleBias;
            public Dictionary<int, LayerInfo> layerDict;
            public LandscapeCell[] cellList;
        }

        struct MaterialInfo
        {
            public string name;
            public int index;
            public SupportedMaterialType type;
            public string[] texNames;
            public float[] scalarParams;
        }

        struct StaticMeshInfo
        {
            public string name;
            public string fbxName;
            public Vector3 location;
            public Quaternion rotation;
            public Vector3 scale;
            public MaterialInfo[] materials;
            public string lightMap;
            public Vector4 uvScaleBias;
            public Vector4 scale0;
            public Vector4 add0;
            public Vector4 scale1;
            public Vector4 add1;
            public Vector4 penumbra;
        }

        struct MaterialKey
        {
            public string name;
            public int index;
            public string lightmap;
            public Vector4 uvScaleBias;
        }

        public static byte[] ToByteArray(int num, byte[] bytes, ref int pointer)
        {
            byte[] res = new byte[num];
            for (int i = 0; i < num; ++i)
            {
                res[i] = bytes[pointer + i];
            }
            pointer += num;
            return res;
        }

        public static string ToString(byte[] bytes, ref int pointer)
        {
            string res = "";
            while (bytes[pointer] != 0)
            {
                res += (char)bytes[pointer];
                ++pointer;
            }
            ++pointer;
            return res;
        }

        public static int ToInt32(byte[] bytes, ref int pointer)
        {
            int res = BitConverter.ToInt32(bytes, pointer);
            pointer += 4;
            return res;
        }

        public static Color32 ToColor32(byte[] bytes, ref int pointer)
        {
            Color32 res;
            res.r = bytes[pointer + 2];
            res.g = bytes[pointer + 1];
            res.b = bytes[pointer];
            res.a = bytes[pointer + 3];
            pointer += 4;
            return res;
        }

        public static float ToFloat(byte[] bytes, ref int pointer)
        {
            float res = BitConverter.ToSingle(bytes, pointer);
            pointer += 4;
            return res;
        }

        public static Vector3 ToVector3(byte[] bytes, ref int pointer)
        {
            Vector3 res = new Vector3();
            res.x = ToFloat(bytes, ref pointer);
            res.y = ToFloat(bytes, ref pointer);
            res.z = ToFloat(bytes, ref pointer);
            return res;
        }

        public static Vector4 ToVector4(byte[] bytes, ref int pointer)
        {
            Vector4 res = new Vector4();
            res.x = ToFloat(bytes, ref pointer);
            res.y = ToFloat(bytes, ref pointer);
            res.z = ToFloat(bytes, ref pointer);
            res.w = ToFloat(bytes, ref pointer);
            return res;
        }

        public static Quaternion ToQuaternion(byte[] bytes, ref int pointer)
        {
            Quaternion res = new Quaternion();
            res.x = ToFloat(bytes, ref pointer);
            res.y = ToFloat(bytes, ref pointer);
            res.z = ToFloat(bytes, ref pointer);
            res.w = ToFloat(bytes, ref pointer);
            return res;
        }

        public static Color ToRGB(byte[] bytes, ref int pointer)
        {
            Color res = new Color();
            res.r = ToFloat(bytes, ref pointer);
            res.g = ToFloat(bytes, ref pointer);
            res.b = ToFloat(bytes, ref pointer);
            return res;
        }

        static void TTTT(Transform trans)
        {
            for (int i = 0; i < trans.childCount; ++i)
            {
                string sName = trans.GetChild(i).name;
                if (Ignore(sName) == false)
                {
                    mOldObj.Add(trans.GetChild(i).gameObject);
                }
                if (sName == "Root")
                {
                    mRootObj = trans.GetChild(i);
                }
                else if (sName == "Ground")
                {
                    mGroundObj = trans.GetChild(i);
                }
                if (sName == "Root" || sName == "Ground")
                {
                    TTTT(trans.GetChild(i));
                }
            }
        }

        static string[] IgnoreList =
        {
            "Ground", "Root", "Effect", "Sound",
        };

        static bool Ignore(string name)
        {
            foreach (string element in IgnoreList)
            {
                if (name.Contains(element))
                {
                    return true;
                }
            }
            return false;
        }

        private static void CreateParentNode(GameObject mParent)
        {
            if (mRootObj == null)
            {
                mRootObj = new GameObject("Root").transform;
                mRootObj.transform.parent = mParent.transform;
            }
            if (mGroundObj == null)
            {
                mGroundObj = new GameObject("Ground").transform;
                mGroundObj.transform.parent = mParent.transform;
            }
            if (mEffectObj == null)
            {
                mEffectObj = new GameObject("Effect").transform;
                mEffectObj.transform.parent = mParent.transform;
            }
        }

        private static GameObject EnsureGameObject(string name)
        {
            GameObject res = GameObject.Find(name);
            if (res != null)
            {
                mRootObj = null;
                mOldObj.Clear();
                TTTT(res.transform);
                return res;
            }
            else
            {
                GameObject mLevelObj = new GameObject(name);
                CreateParentNode(mLevelObj);
                return mLevelObj;
            }
        }

        private static SceneEntry EnsureSceneEntry(GameObject root)
        {
            SceneEntry res = root.GetComponent<SceneEntry>();
            if (res == null)
            {
                res = root.AddComponent<SceneEntry>();
            }
            return res;
        }

        private static Texture2D GetTexture(string path, TextureType eType)
        {
            Texture2D pTex = null;
            if (mTextureMap.TryGetValue(path, out pTex))
            {
                return pTex;
            }
            pTex = AssetDatabase.LoadAssetAtPath<Texture2D>(path);
            mTextureMap.Add(path, pTex);
            return pTex;
        }

        private static void AddReflectionProbe(GameObject level, ReflectionProbeInfo probeInfo)
        {
            GameObject probeObj = GameObject.Find(probeInfo.name);
            if (probeObj == null)
            {
                probeObj = new GameObject(probeInfo.name);
            }
            probeObj.transform.localPosition = probeInfo.position;
            probeObj.transform.parent = mRootObj;
            ReflectionProbe component = probeObj.GetComponent<ReflectionProbe>();
            if (component == null)
            {
                component = probeObj.AddComponent<ReflectionProbe>() as ReflectionProbe;
            }
            component.mode = UnityEngine.Rendering.ReflectionProbeMode.Custom;
            component.intensity = probeInfo.brightness;
            component.boxProjection = true;
            component.size = new Vector3(probeInfo.radius * 2, probeInfo.radius * 2, probeInfo.radius * 2);
            component.customBakedTexture = AssetDatabase.LoadAssetAtPath<Texture>(sResPath + level.name + "/EnvMaps/" + probeInfo.name + ".dds");

            mOldObj.Remove(probeObj);
        }

        public static int GetMaterialIndex(Material[] pMatList, string sName)
        {
            string sMatName = string.Empty;
            for (int i = 0; i < pMatList.Length; ++i)
            {
                sMatName = pMatList[i].name;
                if (sName.Contains(sMatName) || sMatName.Contains(sName))
                {
                    return i;
                }
            }

            return -1;
        }

        private static void ReadSceneEntry(SceneEntry pSE, byte[] bytes, ref int pointer)
        {
            int bMainLight = ToInt32(bytes, ref pointer);
            pSE.enableSunLight = bMainLight != 0;
            if (pSE.enableSunLight)
            {
                pSE.sunLightDirection = ToVector3(bytes, ref pointer);
                pSE.sunLightColor = ToVector3(bytes, ref pointer);
            }
            int bMainFog = ToInt32(bytes, ref pointer);
            pSE.enableFog = bMainFog != 0;
            if (pSE.enableFog)
            {
                pSE.fogHeight = ToFloat(bytes, ref pointer);
                pSE.fogDensity = ToFloat(bytes, ref pointer);
                Vector4 fogColor = Vector4.zero;
                fogColor.x = ToFloat(bytes, ref pointer);
                fogColor.y = ToFloat(bytes, ref pointer);
                fogColor.z = ToFloat(bytes, ref pointer);
                fogColor.w = ToFloat(bytes, ref pointer);
                float max = Math.Max(fogColor.x, Math.Max(fogColor.y, fogColor.z));
                if (max > 1)
                {
                    pSE.fogIntensity = max;
                    pSE.fogInscatteringColor.r = fogColor.x / max;
                    pSE.fogInscatteringColor.g = fogColor.y / max;
                    pSE.fogInscatteringColor.b = fogColor.z / max;
                }
                else
                {
                    pSE.fogInscatteringColor.r = fogColor.x;
                    pSE.fogInscatteringColor.g = fogColor.y;
                    pSE.fogInscatteringColor.b = fogColor.z;
                    pSE.fogIntensity = 1;
                }
                pSE.fogHeightFalloff = ToFloat(bytes, ref pointer);
                pSE.fogMaxOpacity = ToFloat(bytes, ref pointer);
                pSE.fogStartDistance = ToFloat(bytes, ref pointer);
                pSE.fogCutoffDistance = ToFloat(bytes, ref pointer);
            }
        }

        private static ReflectionProbeInfo ReadReflectionProbeInfo(byte[] bytes, ref int pointer)
        {
            ReflectionProbeInfo probeInfo = new ReflectionProbeInfo();
            probeInfo.name = ToString(bytes, ref pointer);
            probeInfo.position = ToVector3(bytes, ref pointer);
            probeInfo.offset = ToVector3(bytes, ref pointer);
            probeInfo.brightness = ToFloat(bytes, ref pointer);
            probeInfo.radius = ToFloat(bytes, ref pointer);
            probeInfo.averageBrightness = ToFloat(bytes, ref pointer);
            return probeInfo;
        }

        private static float GetTerrainExpandPatchCount(float LightMapRes, ref int X, ref int Y, int ComponentSize, int LightmapSize, ref int DesiredSize, int LightingLOD = 0)
        {
            if (LightMapRes <= 0) return 0.0f;
            int PixelPaddingX = 4;
            int PixelPaddingY = 4;
            int PatchExpandCountX = (LightMapRes >= 1.0f) ? (int)((float)PixelPaddingX / LightMapRes) : (PixelPaddingX);
            int PatchExpandCountY = (LightMapRes >= 1.0f) ? (int)((float)PixelPaddingY / LightMapRes) : (PixelPaddingY);
            X = Math.Max(1, PatchExpandCountX >> LightingLOD);
            Y = Math.Max(1, PatchExpandCountY >> LightingLOD);
            DesiredSize = (LightMapRes >= 1.0f) ? Math.Min((int)((ComponentSize + 1) * LightMapRes), 4096) : Math.Min((int)((LightmapSize) * LightMapRes), 4096);
            int CurrentSize = (LightMapRes >= 1.0f) ? Math.Min((int)((2 * (X << LightingLOD) + ComponentSize + 1) * LightMapRes), 4096) : Math.Min((int)((2 * (X << LightingLOD) + LightmapSize) * LightMapRes), 4096);
            if (CurrentSize > DesiredSize)
            {
                // Find maximum bit
                int PriorSize = DesiredSize;
                while (DesiredSize > 0)
                {
                    PriorSize = DesiredSize;
                    DesiredSize = DesiredSize & ~(DesiredSize & ~(DesiredSize - 1));
                }

                DesiredSize = PriorSize << 1; // next bigger size
                if (CurrentSize * CurrentSize <= ((PriorSize * PriorSize) << 1))
                {
                    DesiredSize = PriorSize;
                }
            }

            int DestSize = (int)((float)DesiredSize / (float)CurrentSize * ((float)ComponentSize * LightMapRes));
            float LightMapRatio = (float)DestSize / (ComponentSize * LightMapRes) * CurrentSize / DesiredSize;
            return LightMapRatio;
        }

        private static LandscapeInfo ReadLandscapeInfo(int quadPointNum, byte[] bytes, ref int pointer)
        {
            LandscapeInfo landInfo = new LandscapeInfo();
            float lightingResolution = ToFloat(bytes, ref pointer);
            landInfo.name = ToString(bytes, ref pointer);
            landInfo.location = ToVector3(bytes, ref pointer);
            landInfo.scale = ToVector3(bytes, ref pointer);

            int numLayers = ToInt32(bytes, ref pointer);
            landInfo.layerDict = new Dictionary<int, LayerInfo>();
            for (int i = 0; i < numLayers; ++i)
            {
                int idx = ToInt32(bytes, ref pointer);
                LayerInfo info = new LayerInfo();
                info.tiling = ToFloat(bytes, ref pointer);
                info.yaw = ToFloat(bytes, ref pointer);
                info.pitch = ToFloat(bytes, ref pointer);
                info.specular = ToFloat(bytes, ref pointer);
                info.baseMap = ToString(bytes, ref pointer);
                info.normalMap = ToString(bytes, ref pointer);
                landInfo.layerDict.Add(idx, info);
            }

            int PatchExpandCountX = 0;
            int PatchExpandCountY = 0;
            int DesiredSize = 1; // output by GetTerrainExpandPatchCount but not used below
            float LightMapRatio = GetTerrainExpandPatchCount(lightingResolution, ref PatchExpandCountX, ref PatchExpandCountY, quadPointNum - 1, quadPointNum, ref DesiredSize, 0);
            float LightmapLODScaleX = LightMapRatio / (float)(quadPointNum + 2 * PatchExpandCountX);
            float LightmapLODScaleY = LightMapRatio / (float)(quadPointNum + 2 * PatchExpandCountY);
            float LightmapBiasX = PatchExpandCountX * LightmapLODScaleX;
            float LightmapBiasY = PatchExpandCountY * LightmapLODScaleY;
            float LightmapScaleX = LightmapLODScaleX * (float)(quadPointNum - 1) / (quadPointNum - 1);
            float LightmapScaleY = LightmapLODScaleY * (float)(quadPointNum - 1) / (quadPointNum - 1);
            landInfo.lightMapScaleBias = new Vector4(-LightmapScaleX, LightmapScaleY, LightmapBiasY, LightmapBiasX);

            int cellNum = ToInt32(bytes, ref pointer);
            landInfo.cellList = new LandscapeCell[cellNum];
            for (int i = 0; i < cellNum; ++i)
            {
                landInfo.cellList[i].x = ToInt32(bytes, ref pointer);
                landInfo.cellList[i].y = ToInt32(bytes, ref pointer);
                int bHasLightMap = ToInt32(bytes, ref pointer);
                if (bHasLightMap != 0)
                {
                    landInfo.cellList[i].lightMap = ToString(bytes, ref pointer);
                    Vector4 scaleBias = ToVector4(bytes, ref pointer);
                    landInfo.cellList[i].uvScaleBias = new Vector4(
                        scaleBias.x * landInfo.lightMapScaleBias.x,
                        scaleBias.y * landInfo.lightMapScaleBias.y,
                        scaleBias.x * landInfo.lightMapScaleBias.z + scaleBias.z,
                        scaleBias.y * landInfo.lightMapScaleBias.w + scaleBias.w);

                    landInfo.cellList[i].scale0 = ToVector4(bytes, ref pointer);
                    landInfo.cellList[i].add0 = ToVector4(bytes, ref pointer);
                    landInfo.cellList[i].scale1 = ToVector4(bytes, ref pointer);
                    landInfo.cellList[i].add1 = ToVector4(bytes, ref pointer);
                }
                else
                {
                    landInfo.cellList[i].lightMap = null;
                }
                {
                    int bHasShadowMap = ToInt32(bytes, ref pointer);
                    if (bHasShadowMap != 0)
                    {
                        landInfo.cellList[i].penumbra = ToVector4(bytes, ref pointer);
                    }
                    else
                    {
                        landInfo.cellList[i].penumbra = Vector4.one;
                    }
                }

                landInfo.cellList[i].heightNormalMap = new Color32[quadPointNum * quadPointNum];
                for (int j = 0; j < quadPointNum; ++j)
                {
                    for (int k = 0; k < quadPointNum; ++k)
                    {
                        landInfo.cellList[i].heightNormalMap[j * quadPointNum + k] = ToColor32(bytes, ref pointer);
                    }
                }

                int layerNum = ToInt32(bytes, ref pointer);
                landInfo.cellList[i].layerNumber = layerNum;
                landInfo.cellList[i].layerIndexes = new int[layerNum];
                for (int j = 0; j < layerNum; ++j)
                {
                    landInfo.cellList[i].layerIndexes[j] = ToInt32(bytes, ref pointer);
                }
                landInfo.cellList[i].weightMaps = ToByteArray(quadPointNum * quadPointNum * layerNum, bytes, ref pointer);
                landInfo.cellList[i].Build(quadPointNum);
            }
            return landInfo;
        }

        private static StaticMeshInfo ReadStaticMeshInfo(byte[] bytes, ref int pointer)
        {
            StaticMeshInfo meshInfo = new StaticMeshInfo();
            meshInfo.name = ToString(bytes, ref pointer);

            meshInfo.fbxName = ToString(bytes, ref pointer);
            meshInfo.location = ToVector3(bytes, ref pointer);
            Vector3 euler = ToVector3(bytes, ref pointer);
            meshInfo.rotation = Quaternion.AngleAxis(euler.z, Vector3.up) * Quaternion.AngleAxis(euler.x, Vector3.right) * Quaternion.AngleAxis(euler.y, Vector3.back) * Quaternion.AngleAxis(-90, Vector3.right);
            meshInfo.scale = ToVector3(bytes, ref pointer);
            int numMaterial = ToInt32(bytes, ref pointer);
            meshInfo.materials = new MaterialInfo[numMaterial];
            for (int j = 0; j < numMaterial; ++j)
            {
                meshInfo.materials[j].name = ToString(bytes, ref pointer);
                meshInfo.materials[j].index = ToInt32(bytes, ref pointer);
                meshInfo.materials[j].type = (SupportedMaterialType)ToInt32(bytes, ref pointer);
                {
                    meshInfo.materials[j].texNames = null;
                    int num = ToInt32(bytes, ref pointer);
                    if (num > 0)
                    {
                        meshInfo.materials[j].texNames = new string[num];
                        for (int k = 0; k < num; ++k)
                        {
                            meshInfo.materials[j].texNames[k] = ToString(bytes, ref pointer);
                        }
                    }
                }
                {
                    meshInfo.materials[j].scalarParams = null;
                    int num = ToInt32(bytes, ref pointer);
                    if (num > 0)
                    {
                        meshInfo.materials[j].scalarParams = new float[num];
                        for (int k = 0; k < num; ++k)
                        {
                            meshInfo.materials[j].scalarParams[k] = ToFloat(bytes, ref pointer);
                        }
                    }
                }
            }

            {
                int bHasLightMap = ToInt32(bytes, ref pointer);
                if (bHasLightMap != 0)
                {
                    meshInfo.lightMap = ToString(bytes, ref pointer);
                    meshInfo.uvScaleBias = ToVector4(bytes, ref pointer);
                    meshInfo.scale0 = ToVector4(bytes, ref pointer);
                    meshInfo.add0 = ToVector4(bytes, ref pointer);
                    meshInfo.scale1 = ToVector4(bytes, ref pointer);
                    meshInfo.add1 = ToVector4(bytes, ref pointer);
                }
                else
                {
                    meshInfo.lightMap = null;
                }
            }
            {
                int bHasShadowMap = ToInt32(bytes, ref pointer);
                if (bHasShadowMap != 0)
                {
                    meshInfo.penumbra = ToVector4(bytes, ref pointer);
                }
                else
                {
                    meshInfo.penumbra = Vector4.one;
                }
            }
            return meshInfo;
        }

        private static void AddStaticMesh(GameObject level, StaticMeshInfo meshInfo, ref Dictionary<string, int> matNameDict)
        {
            string fbxPath = sResPath + level.name + "/Meshes/" + meshInfo.fbxName + ".fbx";
            GameObject meshObj = GameObject.Find(meshInfo.name);
            if (meshObj == null)
            {
                meshObj = GameObject.Instantiate(AssetDatabase.LoadAssetAtPath<GameObject>(fbxPath));
                meshObj.name = meshInfo.name;
                meshObj.transform.parent = mRootObj == null ? level.transform : mRootObj;
            }
            else
            {
                mOldObj.Remove(meshObj);
            }
            meshObj.transform.localPosition = meshInfo.location;
            meshObj.transform.localRotation = meshInfo.rotation;
            meshObj.transform.localScale = meshInfo.scale;

            Material[] materialAry = new Material[meshInfo.materials.Length];
            for (int i = (0); i < meshInfo.materials.Length; ++i)
            {
                MaterialKey key = new MaterialKey();
                key.name = meshInfo.materials[i].name;
                key.index = meshInfo.materials[i].index;
                key.lightmap = meshInfo.lightMap;
                key.uvScaleBias = meshInfo.uvScaleBias;

                string sMatName = key.name;

                int index = 0;
                if (matNameDict.ContainsKey(sMatName))
                {
                    index = (++matNameDict[sMatName]);
                }
                else
                {
                    matNameDict[sMatName] = index;
                }

                string matPath = sResPath + level.name + "/Materials/" + sMatName + "_" + index.ToString() + ".mat";
                Material matObj = AssetDatabase.LoadAssetAtPath<Material>(matPath);
                if (matObj == null)
                {
                    matObj = new Material(Shader.Find("Physical Shading/Scene/Plain"));
                    AssetDatabase.CreateAsset(matObj, matPath);
                }
                if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PLAIN)
                {
                    matObj.shader = (sMatName.EndsWith("floor")) ? Shader.Find("Physical Shading/Scene/PlainFloor") : Shader.Find("Physical Shading/Scene/Plain");
                    matObj.mainTexture = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);

                    if (sMatName.EndsWith("floor"))
                    {
                        meshObj.transform.parent = mGroundObj;
                        if (meshObj.GetComponent<MeshCollider>() == null)
                        {
                            meshObj.AddComponent<MeshCollider>();
                        }
                    }
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PLAIN_ALPHA)
                {
                    matObj.shader = Shader.Find("Physical Shading/Scene/PlainAlpha");
                    matObj.mainTexture = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PBR)
                {
                    matObj.shader = (sMatName.EndsWith("floor")) ? Shader.Find("Physical Shading/Scene/PBRFloor") : Shader.Find("Physical Shading/Scene/PBR");

                    matObj.mainTexture = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                    matObj.SetTexture("_MixTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX));
                    matObj.SetTexture("_NrmTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL));

                    if (sMatName.EndsWith("floor"))
                    {
                        meshObj.transform.parent = mGroundObj;
                        if (meshObj.GetComponent<MeshCollider>() == null)
                        {
                            meshObj.AddComponent<MeshCollider>();
                        }
                    }
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PBR_ALPHA)
                {
                    matObj.shader = Shader.Find("Physical Shading/Scene/PBRAlpha");
                    matObj.mainTexture = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                    matObj.SetTexture("_MixTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX));
                    matObj.SetTexture("_NrmTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL));
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_TERRAIN_PBR)
                {
                    matObj.shader = Shader.Find("Physical Shading/Scene/TerrainPBR");
                    matObj.mainTexture = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_MIX);
                    matObj.SetTexture("_MixTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX));
                    matObj.SetTexture("_NrmTex", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL));
                    matObj.SetTexture("_BaseLayer0", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[3] + ".tga", TextureType.TEX_MIX));
                    matObj.SetTexture("_BaseLayer1", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[4] + ".tga", TextureType.TEX_MIX));
                    matObj.SetTexture("_Blend", GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[5] + ".tga", TextureType.TEX_BASE));
                    matObj.SetVector("_Tiling", new Vector3(meshInfo.materials[i].scalarParams[0], meshInfo.materials[i].scalarParams[1], meshInfo.materials[i].scalarParams[2]));
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_WATER)
                {
                    matObj.shader = Shader.Find("Physical Shading/Effect/Water");
                    matObj.SetTexture("_SmallWaveMap", GetTexture("Assets/Levels/Water/WaterNormals.png", TextureType.TEX_MIX));
                    matObj.SetTexture("_LargeWaveMap", GetTexture("Assets/Levels/Water/LargeWaves.png", TextureType.TEX_MIX));
                }
                else
                {
                    matObj.shader = Shader.Find("Physical Shading/Scene/Plain");
                    Texture2D baseTex = GetTexture(sResPath + level.name + "/Textures/" + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                    matObj.mainTexture = baseTex;
                }

                matObj.SetTexture("_LightMap", GetTexture(sResPath + level.name + "/LightMaps/" + meshInfo.lightMap + ".tga", TextureType.TEX_LIGHTMAP));
                matObj.SetVector("_LightMapUVParams", meshInfo.uvScaleBias);
                matObj.SetVector("_LightMapScale0", meshInfo.scale0);
                matObj.SetVector("_LightMapAdd0", meshInfo.add0);
                matObj.SetVector("_LightMapScale1", meshInfo.scale1);
                matObj.SetVector("_LightMapAdd1", meshInfo.add1);

                if (key.index >= materialAry.Length || key.index < 0)
                {
                    int kkk = 0;
                }
                materialAry[key.index] = matObj;
            }

            Renderer meshRenderer = meshObj.GetComponent<Renderer>();
            if (meshObj.transform.childCount > 0)
            {
                for (int n = 0; n < meshObj.transform.childCount; ++n)
                {
                    meshRenderer = meshObj.transform.GetChild(n).GetComponent<Renderer>();
                    Material[] pMatList = new Material[meshRenderer.sharedMaterials.Length];

                    for (int i = (0); i < meshRenderer.sharedMaterials.Length; ++i)
                    {
                        string sMatName = meshRenderer.sharedMaterials[i].name;
                        int vIndex = GetMaterialIndex(materialAry, sMatName);
                        if (vIndex != -1)
                        {
                            pMatList[i] = materialAry[vIndex];
                        }
                    }
                    meshRenderer.sharedMaterials = pMatList;
                }
            }
            else
            {
                meshRenderer.sharedMaterials = materialAry;
            }
            meshRenderer.sharedMaterials = materialAry;
        }

        private static void FormatTexture(string path, TextureType eType)
        {
            if (mFinishList.Contains(path))
            {
                return;
            }
            mFinishList.Add(path);

            TextureImporter tex = TextureImporter.GetAtPath(path) as TextureImporter;
            tex.textureShape = TextureImporterShape.Texture2D;
            tex.sRGBTexture = true;
            TextureImporterFormat eFormat;
            if (eType == TextureType.TEX_BASE)
            {
                eFormat = TextureImporterFormat.ASTC_RGBA_5x5;
                tex.alphaSource = TextureImporterAlphaSource.FromInput;
                tex.alphaIsTransparency = true;
            }
            else if (eType == TextureType.TEX_LIGHTMAP)
            {
                eFormat = TextureImporterFormat.ASTC_RGBA_5x5;
                tex.alphaSource = TextureImporterAlphaSource.FromInput;
                tex.alphaIsTransparency = false;
            }
            else
            {
                eFormat = TextureImporterFormat.ASTC_RGB_5x5;
                tex.alphaSource = TextureImporterAlphaSource.None;
                tex.alphaIsTransparency = false;
            }
            tex.isReadable = false;
            tex.mipmapEnabled = true;
            tex.borderMipmap = false;
            tex.mipmapFilter = TextureImporterMipFilter.KaiserFilter;
            tex.fadeout = false;
            tex.wrapMode = TextureWrapMode.Repeat;
            tex.filterMode = FilterMode.Trilinear;
            tex.anisoLevel = 16;
            tex.maxTextureSize = 1024;
            tex.textureCompression = TextureImporterCompression.Uncompressed;
            TextureImporterPlatformSettings setting = new TextureImporterPlatformSettings();
            setting.overridden = true;
            setting.maxTextureSize = 1024;
            setting.format = eFormat;
            setting.compressionQuality = 100;
            setting.name = "iPhone";
            tex.SetPlatformTextureSettings(setting);
            setting.name = "Android";
            tex.SetPlatformTextureSettings(setting);
            tex.SaveAndReimport();
        }

        private static void TraversalMesh(string nLevelName, StaticMeshInfo meshInfo)
        {
            string sRootPath = sResPath + nLevelName + "/Textures/";

            for (int i = (0); i < meshInfo.materials.Length; ++i)
            {
                if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PLAIN)
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PLAIN_ALPHA)
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PBR)
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_SCENE_PBR_ALPHA)
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_TERRAIN_PBR)
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[1] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[2] + ".tga", TextureType.TEX_NORMAL);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[3] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[4] + ".tga", TextureType.TEX_MIX);
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[5] + ".tga", TextureType.TEX_BASE);
                }
                else if (meshInfo.materials[i].type == SupportedMaterialType.MAT_WATER)
                {
                    FormatTexture("Assets/Levels/Water/WaterNormals.png", TextureType.TEX_MIX);
                    FormatTexture("Assets/Levels/Water/LargeWaves.png", TextureType.TEX_MIX);
                }
                else
                {
                    FormatTexture(sRootPath + meshInfo.materials[i].texNames[0] + ".tga", TextureType.TEX_BASE);
                }

                FormatTexture(sResPath + nLevelName + "/LightMaps/" + meshInfo.lightMap + ".tga", TextureType.TEX_LIGHTMAP);
            }
        }

        private static float DecodeHeight(Color col)
        {
            return ((col.r * 255.0f * 256.0f + col.g * 255.0f) - 32768.0f) / 128.0f;
        }

        private static Vector3 DecodeNormal(Color col)
        {
            float x = -(col.b * 2.0f - 1.0f);
            float z = col.a * 2.0f - 1.0f;
            float y = (float)Math.Sqrt((double)(1.0f - x * x - z * z));

            return new Vector3(x, y, z);
        }

        private static void AddLandscapeCell(GameObject level, int cellQuad,
            ref LandscapeCell cell, ref LandscapeInfo info, ref GameObject root)
        {
            int cellPoint = cellQuad + 1;
            string name = "cell_" + cell.x + "_" + cell.y;
            GameObject cellObj = GameObject.Find(name);
            if (cellObj == null)
            {
                cellObj = new GameObject(name);
            }
            cellObj.transform.parent = root.transform;
            cellObj.transform.localPosition = info.location;
            cellObj.transform.localPosition += new Vector3(
                -cell.x * (float)cellQuad * info.scale.x,
                0,
                cell.y * (float)cellQuad * info.scale.y);
            cellObj.transform.localScale = info.scale;

            Vector3[] vertexBuffer = new Vector3[cellPoint * cellPoint + cell.expandedVertexNum];
            Vector3[] normalBuffer = new Vector3[cellPoint * cellPoint + cell.expandedVertexNum];
            Color32[] colors = new Color32[cellPoint * cellPoint + cell.expandedVertexNum];
            for (int i = 0; i < cellPoint; ++i)
            {
                for (int j = 0; j < cellPoint; ++j)
                {
                    int idx = i * cellPoint + j;
                    vertexBuffer[idx].x = (float)-j;
                    vertexBuffer[idx].y = DecodeHeight(cell.heightNormalMap[idx]);
                    vertexBuffer[idx].z = (float)i;
                    normalBuffer[idx] = DecodeNormal(cell.heightNormalMap[idx]);
                    var vertex = cell.vertexInfoArray[idx];
                    for (int k = 0; k < vertex.copyNum; ++k)
                    {
                        vertexBuffer[vertex.copyStart + k].x = (float)-j;
                        vertexBuffer[vertex.copyStart + k].y = DecodeHeight(cell.heightNormalMap[idx]);
                        vertexBuffer[vertex.copyStart + k].z = (float)i;
                        normalBuffer[vertex.copyStart + k] = DecodeNormal(cell.heightNormalMap[idx]);
                    }
                    int cc = 0;
                    byte[] ct = new byte[4];
                    for (int k = 0; k < cell.vertexLayerArray.Length; ++k)
                    {
                        var layer = cell.vertexLayerArray[k];
                        if ((vertex.copy & (1 << k)) != 0)
                        {
                            int idd = idx;
                            if (cc > 0)
                            {
                                idd = vertex.copyStart + cc - 1;
                            }
                            for (int l = 0; l < 4; ++l)
                            {
                                if (layer.Get(l) < cell.layerNumber)
                                {
                                    ct[l] = cell.weightMaps[idx * cell.layerNumber + layer.Get(l)];
                                }
                                else
                                {
                                    ct[l] = 0;
                                }
                            }
                            colors[idd] = new Color32(ct[0], ct[1], ct[2], ct[3]);
                            ++cc;
                        }

                    }
                }
            }

            Mesh cellMesh = new Mesh();
            cellMesh.name = name + "_mesh";
            cellMesh.vertices = vertexBuffer;
            cellMesh.normals = normalBuffer;
            cellMesh.colors32 = colors;
            cellMesh.subMeshCount = cell.layerTypeArray.Length;
            for (int i = 0; i < cell.layerTypeArray.Length; ++i)
            {
                var type = cell.layerTypeArray[i];
                int[] vertex = new int[4];
                List<int> trangles = new List<int>();
                for (int j = 0; j < cellQuad; ++j)
                {
                    for (int k = 0; k < cellQuad; ++k)
                    {
                        if (cell.layerQuadMap[j * cellQuad + k] == type.layers)
                        {
                            for (int l = 0; l < 2; ++l)
                            {
                                for (int m = 0; m < 2; ++m)
                                {
                                    int v = (j + l) * cellPoint + k + m;
                                    var vinfo = cell.vertexInfoArray[v];
                                    int cc = 0;
                                    for (int n = 0; n < type.vertexCopy; ++n)
                                    {
                                        if ((vinfo.copy & (1 << n)) != 0)
                                        {
                                            ++cc;
                                        }
                                    }
                                    if (cc > 0)
                                    {
                                        cc -= 1;
                                        v = vinfo.copyStart + cc;
                                    }
                                    vertex[l * 2 + m] = v;
                                }
                            }
                            trangles.Add(vertex[0]);
                            trangles.Add(vertex[1]);
                            trangles.Add(vertex[2]);
                            trangles.Add(vertex[2]);
                            trangles.Add(vertex[1]);
                            trangles.Add(vertex[3]);
                        }

                    }
                }
                
                cellMesh.SetTriangles(trangles.ToArray(), i);
            }

            cellMesh.RecalculateBounds();
            MeshFilter meshFilter = cellObj.GetComponent<MeshFilter>();
            if (meshFilter == null)
            {
                meshFilter = cellObj.AddComponent<MeshFilter>();
            }
            meshFilter.sharedMesh = cellMesh;

            var materials = new Material[cell.layerTypeArray.Length];
            for (int i = 0; i < cell.layerTypeArray.Length; ++i)
            {
                var layerType = cell.layerTypeArray[i];
                var layerWeight = cell.vertexLayerArray[layerType.vertexCopy];
                List<Vector4> layerMask = new List<Vector4>();
                List<int> layerNum = new List<int>();
                int layerIdx = 0;
                for (int j = 0; j < 16; ++j)
                {
                    if ((layerType.layers & (1 << j)) != 0)
                    {
                        Vector4 mask = new Vector4(0, 0, 0, 0);
                        if (layerWeight.Get(0) == j)
                        {
                            mask.x = 1;
                        }
                        else if (layerWeight.Get(1) == j)
                        {
                            mask.y = 1;
                        }
                        else if (layerWeight.Get(2) == j)
                        {
                            mask.z = 1;
                        }
                        else if (layerWeight.Get(3) == j)
                        {
                            mask.w = 1;
                        }
                        layerMask.Add(mask);
                        layerNum.Add(j);
                        ++layerIdx;
                    }
                }

                string shaderName = "VenusRendering/Landscape/Landscape_" + layerMask.Count;
                string matPath = sResPath + level.name + "/Materials/" + info.name + "_" + cell.x + "_" + cell.y + "_" + layerType.layers + ".mat";
                Material matObj = AssetDatabase.LoadAssetAtPath<Material>(matPath);
                if (matObj == null)
                {
                    matObj = new Material(Shader.Find(shaderName));
                    AssetDatabase.CreateAsset(matObj, matPath);
                }
                matObj.shader = Shader.Find(shaderName);

                for (int j = 0; j < layerMask.Count; ++j)
                {
                    matObj.SetVector("_LayerMask" + j, layerMask[j]);
                    var layer = info.layerDict[cell.layerIndexes[layerNum[j]]];
                    matObj.SetVector("_LayerParams" + j, new Vector4(layer.tiling, layer.yaw, layer.pitch, layer.specular));
                    matObj.SetTexture("_BaseMap" + j, GetTexture(sResPath + level.name + "/Textures/" + layer.baseMap + ".tga", TextureType.TEX_BASE));
                    matObj.SetTexture("_NormalMap" + j, GetTexture(sResPath + level.name + "/Textures/" + layer.normalMap + ".tga", TextureType.TEX_NORMAL));
                }

                matObj.SetTexture("_LightMap", GetTexture(sResPath + level.name + "/LightMaps/" + cell.lightMap + ".tga", TextureType.TEX_LIGHTMAP));
                matObj.SetVector("_Lighting", info.lightMapScaleBias);
                matObj.SetVector("_LightMapUVParams", cell.uvScaleBias);
                matObj.SetVector("_LightMapScale0", cell.scale0);
                matObj.SetVector("_LightMapAdd0", cell.add0);
                matObj.SetVector("_LightMapScale1", cell.scale1);
                matObj.SetVector("_LightMapAdd1", cell.add1);
                materials[i] = matObj;
            }

            MeshRenderer meshRenderer = cellObj.GetComponent<MeshRenderer>();
            if (meshRenderer == null)
            {
                meshRenderer = cellObj.AddComponent<MeshRenderer>();
            }
            meshRenderer.sharedMaterials = materials;
        }

        private static void AddLandscape(GameObject level, int cellQuad,
            ref LandscapeInfo info, ref GameObject root)
        {
            for (int i = 0; i < info.cellList.Length; ++i)
            {
                AddLandscapeCell(level, cellQuad, ref info.cellList[i], ref info, ref root);
            }
        }

        private static void ImportLevel(string name, byte[] bytes)
        {
            mTextureMap.Clear();
            mUseTime = Time.realtimeSinceStartup;

            int pointer = 0;
            GameObject level = EnsureGameObject(name);
            SceneEntry entry = EnsureSceneEntry(level);
            ReadSceneEntry(entry, bytes, ref pointer);

            int numRef = ToInt32(bytes, ref pointer);
            for (int i = 0; i < numRef; ++i)
            {
                ReflectionProbeInfo probeInfo = ReadReflectionProbeInfo(bytes, ref pointer);
                AddReflectionProbe(level, probeInfo);
            }

            int numObj = ToInt32(bytes, ref pointer);
            if (numObj > 0)
            {
                Dictionary<string, int> matNameDict = new Dictionary<string, int>();
                EditorApplication.CallbackFunction myUpdate = null;
                int nIndex = 0;
                myUpdate = () =>
                {
                    StaticMeshInfo meshInfo = ReadStaticMeshInfo(bytes, ref pointer);

                    bool isFinish = EditorUtility.DisplayCancelableProgressBar("正在处理(" + nIndex + "/" + numObj + ")", meshInfo.name, (float)nIndex++ / (float)numObj);
                    AddStaticMesh(level, meshInfo, ref matNameDict);

                    if (isFinish || nIndex >= numObj)
                    {
                        EditorUtility.ClearProgressBar();
                        EditorApplication.update -= myUpdate;

                        if (nIndex >= numObj)
                        {
                            for (int i = 0; i < mOldObj.Count; ++i)
                            {
                                GameObject.DestroyImmediate(mOldObj[i], true);
                            }
                            mOldObj.Clear();

                            // 地形相关
                            int landComponentSizeQuads = ToInt32(bytes, ref pointer);
                            if (landComponentSizeQuads > 0)
                            {
                                LandscapeInfo landInfo = ReadLandscapeInfo(landComponentSizeQuads + 1, bytes, ref pointer);
                                GameObject landObj = GameObject.Find(landInfo.name);
                                if (landObj == null)
                                {
                                    landObj = new GameObject(landInfo.name);
                                }
                                landObj.transform.parent = mGroundObj;
                                AddLandscape(level, landComponentSizeQuads, ref landInfo, ref landObj);
                                Debug.Log("Landscape: " + landInfo.name);
                            }
                            entry.SetLandComponentSizeQuads(landComponentSizeQuads);
                            entry.UpdateParams();
                        }

                        Debug.Log("耗时: " + (Time.realtimeSinceStartup - mUseTime));
                    }
                };
                EditorApplication.update += myUpdate;
            }
            else
            {
                int landComponentSizeQuads = ToInt32(bytes, ref pointer);
                if (landComponentSizeQuads > 0)
                {
                    LandscapeInfo landInfo = ReadLandscapeInfo(landComponentSizeQuads + 1, bytes, ref pointer);
                    GameObject landObj = GameObject.Find(landInfo.name);
                    if (landObj == null)
                    {
                        landObj = new GameObject(landInfo.name);
                    }
                    landObj.transform.parent = mGroundObj;
                    AddLandscape(level, landComponentSizeQuads, ref landInfo, ref landObj);
                    Debug.Log("Landscape: " + landInfo.name);
                }
                entry.SetLandComponentSizeQuads(landComponentSizeQuads);
                entry.UpdateParams();
            }
        }

        [MenuItem("VenusRendering/ImportLevel")]
        public static void MenuClicked()
        {
            OpenFileName ofn = new OpenFileName();
            ofn.structSize = Marshal.SizeOf(ofn);
            ofn.filter = "Levels\0*.level\0\0";
            ofn.file = new string(new char[256]);
            ofn.maxFile = ofn.file.Length;
            ofn.fileTitle = new string(new char[64]);
            ofn.maxFileTitle = ofn.fileTitle.Length;
            ofn.initialDir = Application.dataPath;
            ofn.title = "Open File";
            ofn.defExt = "level";
            ofn.flags = 0x00080000 | 0x00001000 | 0x00000800 | 0x00000200 | 0x00000008;
            if (DllHelper.GetOpenFileName(ofn))
            {
                ImportLevel(Path.GetFileNameWithoutExtension(ofn.file), File.ReadAllBytes(ofn.file));
            }
        }

        [MenuItem("VenusRendering/TextureSettings")]
        public static void TextureSettings()
        {
            string[] files = Directory.GetFiles("Assets/Levels", "*.level", SearchOption.AllDirectories);
            for (int n = 0; n < files.Length; ++n)
            {
                mFinishList.Clear();

                string sLevelName = Path.GetFileNameWithoutExtension(files[n]);
                byte[] bytes = File.ReadAllBytes(files[n]);
                
                mUseTime = Time.realtimeSinceStartup;

                int pointer = 0;
                SceneEntry entry = new SceneEntry();
                ReadSceneEntry(entry, bytes, ref pointer);
                entry.UpdateParams();

                int numRef = ToInt32(bytes, ref pointer);
                for (int i = 0; i < numRef; ++i)
                {
                    ReadReflectionProbeInfo(bytes, ref pointer);
                }
                
                int numObj = ToInt32(bytes, ref pointer);
                for (int i = 0; i < numObj; ++i)
                {
                    StaticMeshInfo meshInfo = ReadStaticMeshInfo(bytes, ref pointer);
                    EditorUtility.DisplayProgressBar("场景:" + sLevelName + "       (" + i + "/" + numObj + ")", meshInfo.name, (float)i++ / (float)numObj);
                    TraversalMesh(sLevelName, meshInfo);
                }

                EditorUtility.ClearProgressBar();
            }
        }
    }
}
