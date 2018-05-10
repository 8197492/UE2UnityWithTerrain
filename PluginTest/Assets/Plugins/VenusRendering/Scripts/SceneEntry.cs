using UnityEngine;

namespace VenusRendering
{
    [DisallowMultipleComponent]
    [AddComponentMenu("VenusRendering/SceneEntry")]
    [ExecuteInEditMode]
    public class SceneEntry : MonoBehaviour
    {
        public bool enableSunLight = false;
        public Vector3 sunLightDirection = new Vector3(0.0f, 1.0f, 0.0f);
        public Vector3 sunLightColor = new Vector3(1.0f, 1.0f, 1.0f);
        [Range(0.0f, 1.0f)]
        public float sunLightIntensityHDR = 1.0f;
        [Range(0.0f, 1.0f)]
        public float sunLightIntensityLDR = 1.0f;
        public Vector2 sunLightPenumbra = new Vector2(20.0f, 20.0f);

        public Vector3 roleLighting = new Vector3(1.0f, 1.0f, 1.0f);

        public bool enableFog = false;
        public float fogHeight = 0.0f;
        public float fogDensity = 0.02f;
        public Color fogInscatteringColor = new Color(0.5f, 0.5f, 0.5f);
        public float fogIntensity = 1.0f;
        public float fogHeightFalloff = 0.2f;
        [Range(0.0f, 1.0f)]
        public float fogMaxOpacity = 1.0f;
        public float fogStartDistance = 0.0f;
        public float fogCutoffDistance = 0.0f;

		[Range(0.0f, 1.0f)]
        public float bloomThreshold = 0.5f;
        [Range(0.0f, 1.0f)]
        public float bloomScale = 0.25f;
        [Range(0.0f, 1.0f)]
        public float bloomThresholdHDR = 0.5f;
        [Range(0.0f, 1.0f)]
        public float bloomScaleHDR = 0.25f;
        [Range(0.0f, 100.0f)]
        public float exposure = 1.0f;

		public bool enableBloomHints = false;
		public Color bloomHint0 = Color.white;
		[Range(0.0f, 2.0f)]
		public float bloomWeight0 = 0.225f;
		public Color bloomHint1 = Color.white;
		[Range(0.0f, 2.0f)]
		public float bloomWeight1 = 0.575f;
		public Color bloomHint2 = Color.white;
		[Range(0.0f, 2.0f)]
		public float bloomWeight2 = 1.0f;
		public Color bloomHint3 = Color.white;
		[Range(0.0f, 2.0f)]
		public float bloomWeight3 = 1.0f;

		public bool combinedRGB = true;
        public AnimationCurve combinedChannels = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
        public AnimationCurve redChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
        public AnimationCurve greenChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
        public AnimationCurve blueChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));

		public bool combinedHDRRGB = true;
		public AnimationCurve combinedHDRChannels = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
		public AnimationCurve redHDRChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
		public AnimationCurve greenHDRChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));
		public AnimationCurve blueHDRChannel = new AnimationCurve(new Keyframe(0f, 0f), new Keyframe(1f, 1f));

		private Texture2D colorLUT = null;
		private Texture3D colorLUT3D = null;

		private Texture2D colorHDRLUT = null;
		private Texture3D colorHDRLUT3D = null;

        private float landComponentSizeQuads = 0;

#if UNITY_EDITOR
        private void Awake()
        {
            UpdateParams();
        }

        private void OnValidate()
        {
            UpdateParams();
        }
#else
        private void OnEnable()
        {
            UpdateParams();
        }
#endif

        public void SetLandComponentSizeQuads(int sizeQuads)
        {
            landComponentSizeQuads = (float)sizeQuads;
        }

        public void UpdateParams()
        {
            if (enableSunLight)
            {
                Shader.EnableKeyword("SUNLIGHT_ON");
                Shader.SetGlobalVector("_sunLightDirection", sunLightDirection.normalized);
                Shader.SetGlobalVector("_sunLightColor", new Vector4(
                    sunLightColor.x * sunLightIntensityHDR,
                    sunLightColor.y * sunLightIntensityHDR,
                    sunLightColor.z * sunLightIntensityHDR,
                    sunLightIntensityLDR / sunLightIntensityHDR));
                Shader.SetGlobalVector("_sunLightPenumbra", new Vector4(sunLightPenumbra.x, sunLightPenumbra.y, 0, 0));
            }
            else
            {
                Shader.DisableKeyword("SUNLIGHT_ON");
            }

            Shader.SetGlobalVector("_roleLighting", roleLighting);

            RenderPipeline.SetFogEnable(enableFog);
            if (enableFog)
            {
                float fogDensityFinal = fogDensity * 0.001f;
                float fogHeightFalloffFinal = fogHeightFalloff * 0.001f;
                Shader.EnableKeyword("FOG_ON");
                Shader.SetGlobalVector("_FogColorParams", new Vector4(
                    fogInscatteringColor.r * fogIntensity,
                    fogInscatteringColor.g * fogIntensity,
                    fogInscatteringColor.b * fogIntensity,
                    1.0f - fogMaxOpacity));
                Shader.SetGlobalVector("_FogParams3", new Vector4(fogDensityFinal, fogHeight, fogMaxOpacity, fogCutoffDistance));
                RenderPipeline.SetFogParams(fogHeight, fogDensityFinal, fogHeightFalloffFinal, fogStartDistance);
            }
            else
            {
                Shader.DisableKeyword("FOG_ON");
            }

            Shader.SetGlobalVector("_BloomParams", new Vector4(bloomThreshold, bloomScale, bloomThresholdHDR, bloomScaleHDR * 2));
            Shader.SetGlobalVector("_HDRParams", new Vector4(bloomThresholdHDR, bloomScaleHDR * 2, exposure, 1));

            if (!enableBloomHints)
			{
				bloomHint0 = Color.white;
				bloomWeight0 = 0.225f;
				bloomHint1 = Color.white;
				bloomWeight1 = 0.575f;
				bloomHint2 = Color.white;
				bloomWeight2 = 1.0f;
				bloomHint3 = Color.white;
				bloomWeight3 = 1.0f;
			}

			Shader.SetGlobalVector("bloomHint0", new Vector4(0.125f * bloomHint0.r * bloomWeight0, 0.125f * bloomHint0.g * bloomWeight0, 0.125f * bloomHint0.b * bloomWeight0, 0.125f * bloomWeight0));
			Shader.SetGlobalVector("bloomHint1", new Vector4(0.125f * bloomHint1.r * bloomWeight1, 0.125f * bloomHint1.g * bloomWeight1, 0.125f * bloomHint1.b * bloomWeight1, 0.125f * bloomWeight1));
			Shader.SetGlobalVector("bloomHint2", new Vector4(0.125f * bloomHint2.r * bloomWeight2, 0.125f * bloomHint2.g * bloomWeight2, 0.125f * bloomHint2.b * bloomWeight2, 0.125f * bloomWeight2));
			Shader.SetGlobalVector("bloomHint3", new Vector4(0.125f * bloomHint3.r * bloomWeight3, 0.125f * bloomHint3.g * bloomWeight3, 0.125f * bloomHint3.b * bloomWeight3, 0.125f * bloomWeight3));

			if (redChannel != null && greenChannel != null && blueChannel != null)
			{
				if (combinedRGB)
				{
					redChannel.keys = combinedChannels.keys;
					greenChannel.keys = combinedChannels.keys;
					blueChannel.keys = combinedChannels.keys;
				}

				if (combinedHDRRGB)
				{
					redHDRChannel.keys = combinedHDRChannels.keys;
					greenHDRChannel.keys = combinedHDRChannels.keys;
					blueHDRChannel.keys = combinedHDRChannels.keys;
				}

				if (SystemInfo.supports3DTextures && SystemInfo.graphicsDeviceType
					!= UnityEngine.Rendering.GraphicsDeviceType.OpenGLES2)
				{
					Shader.EnableKeyword("LUT_3D");
					Color[] pixels3D = new Color[16 * 16 * 16];

					if (!colorLUT3D)
						colorLUT3D = new Texture3D(16, 16, 16, TextureFormat.ARGB32, false);
					colorLUT3D.hideFlags = HideFlags.DontSave;
					colorLUT3D.wrapMode = TextureWrapMode.Clamp;
					colorLUT3D.filterMode = FilterMode.Bilinear;
					for (int i = 0; i < 16; ++i)
					{
						for (int j = 0; j < 16; ++j)
						{
							for (int k = 0; k < 16; ++k)
							{
								float r = Mathf.Clamp(redChannel.Evaluate(i / 15.0f), 0.0f, 1.0f);
								float g = Mathf.Clamp(greenChannel.Evaluate(j / 15.0f), 0.0f, 1.0f);
								float b = Mathf.Clamp(blueChannel.Evaluate(k / 15.0f), 0.0f, 1.0f);
								pixels3D[i + (j * 16) + (k * 16 * 16)] = new Color(r, g, b);
							}
						}
					}
					colorLUT3D.SetPixels(pixels3D);
					colorLUT3D.Apply();
					Shader.SetGlobalTexture("colorLUT3D", colorLUT3D);
					Shader.SetGlobalTexture("colorLUT", null);
					if (colorLUT)
						colorLUT = null;

					if (!colorHDRLUT3D)
						colorHDRLUT3D = new Texture3D(16, 16, 16, TextureFormat.ARGB32, false);
					colorHDRLUT3D.hideFlags = HideFlags.DontSave;
					colorHDRLUT3D.wrapMode = TextureWrapMode.Clamp;
					colorHDRLUT3D.filterMode = FilterMode.Bilinear;
					for (int i = 0; i < 16; ++i)
					{
						for (int j = 0; j < 16; ++j)
						{
							for (int k = 0; k < 16; ++k)
							{
								float r = Mathf.Clamp(redHDRChannel.Evaluate(i / 15.0f), 0.0f, 1.0f);
								float g = Mathf.Clamp(greenHDRChannel.Evaluate(j / 15.0f), 0.0f, 1.0f);
								float b = Mathf.Clamp(blueHDRChannel.Evaluate(k / 15.0f), 0.0f, 1.0f);
								pixels3D[i + (j * 16) + (k * 16 * 16)] = new Color(r, g, b);
							}
						}
					}
					colorHDRLUT3D.SetPixels(pixels3D);
					colorHDRLUT3D.Apply();
					Shader.SetGlobalTexture("colorHDRLUT3D", colorHDRLUT3D);
					Shader.SetGlobalTexture("colorHDRLUT", null);
					if (colorHDRLUT)
						colorHDRLUT = null;
				}
				else
				{
					Shader.DisableKeyword("LUT_3D");

					if (!colorLUT)
						colorLUT = new Texture2D(256, 16, TextureFormat.ARGB32, false, true);
					colorLUT.hideFlags = HideFlags.DontSave;
					colorLUT.wrapMode = TextureWrapMode.Clamp;
					colorLUT.filterMode = FilterMode.Bilinear;

					for (int i = 0; i < 16; ++i)
					{
						for (int j = 0; j < 16; ++j)
						{
							for (int k = 0; k < 16; ++k)
							{
								float r = Mathf.Clamp(redChannel.Evaluate(i / 15.0f), 0.0f, 1.0f);
								float g = Mathf.Clamp(greenChannel.Evaluate(j / 15.0f), 0.0f, 1.0f);
								float b = Mathf.Clamp(blueChannel.Evaluate(k / 15.0f), 0.0f, 1.0f);

								colorLUT.SetPixel(i + k * 16, j, new Color(r, g, b));
							}
						}
					}
					colorLUT.Apply();
					Shader.SetGlobalTexture("colorLUT", colorLUT);
					Shader.SetGlobalTexture("colorLUT3D", null);
					if (colorLUT3D)
						colorLUT3D = null;

					if (!colorHDRLUT)
						colorHDRLUT = new Texture2D(256, 16, TextureFormat.ARGB32, false, true);
					colorHDRLUT.hideFlags = HideFlags.DontSave;
					colorHDRLUT.wrapMode = TextureWrapMode.Clamp;
					colorHDRLUT.filterMode = FilterMode.Bilinear;

					for (int i = 0; i < 16; ++i)
					{
						for (int j = 0; j < 16; ++j)
						{
							for (int k = 0; k < 16; ++k)
							{
								float r = Mathf.Clamp(redHDRChannel.Evaluate(i / 15.0f), 0.0f, 1.0f);
								float g = Mathf.Clamp(greenHDRChannel.Evaluate(j / 15.0f), 0.0f, 1.0f);
								float b = Mathf.Clamp(blueHDRChannel.Evaluate(k / 15.0f), 0.0f, 1.0f);

								colorHDRLUT.SetPixel(i + k * 16, j, new Color(r, g, b));
							}
						}
					}
					colorHDRLUT.Apply();
					Shader.SetGlobalTexture("colorHDRLUT", colorHDRLUT);
					Shader.SetGlobalTexture("colorHDRLUT3D", null);
					if (colorHDRLUT3D)
						colorHDRLUT3D = null;
				}
			}

            if (landComponentSizeQuads != 0)
            {
                Shader.SetGlobalFloat("_InvLandSizeQuads", 1.0f / landComponentSizeQuads);
            }
        }
    }
}
