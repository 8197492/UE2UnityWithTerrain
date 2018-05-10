using UnityEditor;
using UnityEngine;

namespace VenusRendering
{
	[CustomEditor(typeof(SceneEntry))]
	class SceneEntryEditor : Editor
	{
        SerializedProperty enableSunLight;
        SerializedProperty sunLightDirection;
        SerializedProperty sunLightColor;
        SerializedProperty sunLightIntensityHDR;
        SerializedProperty sunLightIntensityLDR;
        SerializedProperty sunLightPenumbra;

        SerializedProperty roleLighting;

        SerializedProperty enableFog;
        SerializedProperty fogHeight;
        SerializedProperty fogDensity;
        SerializedProperty fogInscatteringColor;
        SerializedProperty fogIntensity;
		SerializedProperty fogHeightFalloff;
		SerializedProperty fogMaxOpacity;
		SerializedProperty fogStartDistance;
		SerializedProperty fogCutoffDistance;

		SerializedProperty bloomThreshold;
		SerializedProperty bloomScale;
		SerializedProperty bloomThresholdHDR;
		SerializedProperty bloomScaleHDR;
		SerializedProperty exposure;

		SerializedProperty enableBloomHints;
		SerializedProperty bloomHint0;
		SerializedProperty bloomWeight0;
		SerializedProperty bloomHint1;
		SerializedProperty bloomWeight1;
		SerializedProperty bloomHint2;
		SerializedProperty bloomWeight2;
		SerializedProperty bloomHint3;
		SerializedProperty bloomWeight3;

		SerializedProperty combinedRGB;
		SerializedProperty combinedChannels;
		SerializedProperty redChannel;
		SerializedProperty greenChannel;
		SerializedProperty blueChannel;

		SerializedProperty combinedHDRRGB;
		SerializedProperty combinedHDRChannels;
		SerializedProperty redHDRChannel;
		SerializedProperty greenHDRChannel;
		SerializedProperty blueHDRChannel;

		void OnEnable()
		{
            enableSunLight = serializedObject.FindProperty("enableSunLight");
            sunLightDirection = serializedObject.FindProperty("sunLightDirection");
            sunLightColor = serializedObject.FindProperty("sunLightColor");
            sunLightIntensityHDR = serializedObject.FindProperty("sunLightIntensityHDR");
            sunLightIntensityLDR = serializedObject.FindProperty("sunLightIntensityLDR");
            sunLightPenumbra = serializedObject.FindProperty("sunLightPenumbra");

            roleLighting = serializedObject.FindProperty("roleLighting");

            enableFog = serializedObject.FindProperty("enableFog");
            fogHeight = serializedObject.FindProperty("fogHeight");
            fogDensity = serializedObject.FindProperty("fogDensity");
            fogInscatteringColor = serializedObject.FindProperty("fogInscatteringColor");
            fogIntensity = serializedObject.FindProperty("fogIntensity");
            fogHeightFalloff = serializedObject.FindProperty("fogHeightFalloff");
            fogMaxOpacity = serializedObject.FindProperty("fogMaxOpacity");
            fogStartDistance = serializedObject.FindProperty("fogStartDistance");
            fogCutoffDistance = serializedObject.FindProperty("fogCutoffDistance");

            bloomThreshold = serializedObject.FindProperty("bloomThreshold");
			bloomScale = serializedObject.FindProperty("bloomScale");
			bloomThresholdHDR = serializedObject.FindProperty("bloomThresholdHDR");
			bloomScaleHDR = serializedObject.FindProperty("bloomScaleHDR");
			exposure = serializedObject.FindProperty("exposure");

			enableBloomHints = serializedObject.FindProperty("enableBloomHints");
			bloomHint0 = serializedObject.FindProperty("bloomHint0");
			bloomWeight0 = serializedObject.FindProperty("bloomWeight0");
			bloomHint1 = serializedObject.FindProperty("bloomHint1");
			bloomWeight1 = serializedObject.FindProperty("bloomWeight1");
			bloomHint2 = serializedObject.FindProperty("bloomHint2");
			bloomWeight2 = serializedObject.FindProperty("bloomWeight2");
			bloomHint3 = serializedObject.FindProperty("bloomHint3");
			bloomWeight3 = serializedObject.FindProperty("bloomWeight3");

			combinedRGB = serializedObject.FindProperty("combinedRGB");
			combinedChannels = serializedObject.FindProperty("combinedChannels");
			redChannel = serializedObject.FindProperty("redChannel");
			greenChannel = serializedObject.FindProperty("greenChannel");
			blueChannel = serializedObject.FindProperty("blueChannel");

			combinedHDRRGB = serializedObject.FindProperty("combinedHDRRGB");
			combinedHDRChannels = serializedObject.FindProperty("combinedHDRChannels");
			redHDRChannel = serializedObject.FindProperty("redHDRChannel");
			greenHDRChannel = serializedObject.FindProperty("greenHDRChannel");
			blueHDRChannel = serializedObject.FindProperty("blueHDRChannel");
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update();

            EditorGUILayout.PropertyField(enableSunLight);
            if (enableSunLight.boolValue)
            {
                EditorGUILayout.PropertyField(sunLightDirection);
                EditorGUILayout.PropertyField(sunLightColor);
                EditorGUILayout.PropertyField(sunLightIntensityHDR);
                EditorGUILayout.PropertyField(sunLightIntensityLDR);
                EditorGUILayout.PropertyField(sunLightPenumbra, new GUIContent("Penumbra(landscape, objects)"));
            }
            EditorGUILayout.Separator();

            EditorGUILayout.PropertyField(roleLighting, new GUIContent("Role(sky,ibl,sun)"));
            EditorGUILayout.Separator();

            EditorGUILayout.PropertyField(enableFog);
            if (enableFog.boolValue)
            {
                EditorGUILayout.PropertyField(fogHeight);
                EditorGUILayout.PropertyField(fogDensity);
                EditorGUILayout.PropertyField(fogInscatteringColor);
                EditorGUILayout.PropertyField(fogIntensity);
                EditorGUILayout.PropertyField(fogHeightFalloff);
                EditorGUILayout.PropertyField(fogMaxOpacity);
                EditorGUILayout.PropertyField(fogStartDistance);
                EditorGUILayout.PropertyField(fogCutoffDistance);
            }
			EditorGUILayout.Separator();

			EditorGUILayout.PropertyField(bloomThreshold);
			EditorGUILayout.PropertyField(bloomScale);
			EditorGUILayout.PropertyField(bloomThresholdHDR);
			EditorGUILayout.PropertyField(bloomScaleHDR);
			EditorGUILayout.PropertyField(exposure);
			EditorGUILayout.Separator();

			EditorGUILayout.PropertyField(enableBloomHints);
			if (enableBloomHints.boolValue)
			{
				EditorGUILayout.PropertyField(bloomHint0);
				EditorGUILayout.PropertyField(bloomWeight0);

				EditorGUILayout.PropertyField(bloomHint1);
				EditorGUILayout.PropertyField(bloomWeight1);

				EditorGUILayout.PropertyField(bloomHint2);
				EditorGUILayout.PropertyField(bloomWeight2);

				EditorGUILayout.PropertyField(bloomHint3);
				EditorGUILayout.PropertyField(bloomWeight3);
			}
			EditorGUILayout.Separator();

			EditorGUILayout.PropertyField(combinedRGB);
			if (combinedRGB.boolValue)
			{
				EditorGUILayout.PropertyField(combinedChannels);
			}
			else
			{
				EditorGUILayout.PropertyField(redChannel);
				EditorGUILayout.PropertyField(greenChannel);
				EditorGUILayout.PropertyField(blueChannel);
			}

			EditorGUILayout.Separator();

			EditorGUILayout.PropertyField(combinedHDRRGB, new GUIContent("Combined HDR RGB"));
			if (combinedHDRRGB.boolValue)
			{
				EditorGUILayout.PropertyField(combinedHDRChannels);
			}
			else
			{
				EditorGUILayout.PropertyField(redHDRChannel);
				EditorGUILayout.PropertyField(greenHDRChannel);
				EditorGUILayout.PropertyField(blueHDRChannel);
			}

			serializedObject.ApplyModifiedProperties();
		}
	}
}
