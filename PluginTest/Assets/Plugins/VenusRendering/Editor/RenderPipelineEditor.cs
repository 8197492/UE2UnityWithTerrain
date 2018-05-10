using UnityEditor;
using UnityEngine;

namespace VenusRendering
{
    [CustomEditor(typeof(RenderPipeline))]
    class RenderPipelineEditor : Editor
    {
        SerializedProperty framerate;
        SerializedProperty matHQ;
        SerializedProperty postFX;
        SerializedProperty resolution;
        SerializedProperty msaa;
#if UNITY_STANDALONE
        SerializedProperty ssaa;
#endif
        SerializedProperty enableHDR;
        SerializedProperty enableBloom;

        void OnEnable()
        {
            framerate = serializedObject.FindProperty("framerate");
            matHQ = serializedObject.FindProperty("matHQ");
            postFX = serializedObject.FindProperty("postFX");
            resolution = serializedObject.FindProperty("resolution");
            msaa = serializedObject.FindProperty("msaa");
#if UNITY_STANDALONE
            ssaa = serializedObject.FindProperty("ssaa");
#endif
            enableHDR = serializedObject.FindProperty("enableHDR");
            enableBloom = serializedObject.FindProperty("enableBloom");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(framerate);
            EditorGUILayout.PropertyField(matHQ, new GUIContent("HQ Materials"));
            EditorGUILayout.PropertyField(postFX, new GUIContent("PostFX"));
            if (postFX.boolValue)
            {
                EditorGUILayout.PropertyField(resolution);
                EditorGUILayout.PropertyField(msaa, new GUIContent("MSAA"));
#if UNITY_STANDALONE
                EditorGUILayout.PropertyField(ssaa, new GUIContent("SSAA"));
#endif
                EditorGUILayout.PropertyField(enableHDR);
                if (!enableHDR.boolValue)
                    EditorGUILayout.PropertyField(enableBloom);
            }

            serializedObject.ApplyModifiedProperties();
        }
    }
}
