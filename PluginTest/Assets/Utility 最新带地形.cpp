#include "Utility.h"

#include "Notifications/NotificationManager.h"
#include "Exporters/Exporter.h"
#include "Public/ObjectTools.h"
#include "Public/HAL/Runnable.h"
#include "Public/HAL/RunnableThread.h"
#include "Public/HAL/PlatformFilemanager.h"
#include "Public/Misc/SingleThreadRunnable.h"
#include "Public/SceneTypes.h"
#include "Public/LightMap.h"
#include "Public/ShadowMap.h"
#include "Public/PrecomputedLightVolume.h"
#include "Private/ScenePrivate.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/ShadowMapTexture2D.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Landscape.h"
#include "Landscapeinfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGizmoActiveActor.h"
#include "LandscapeComponent.h"
#include "ring_buffer.h"
#include "PVR.h"
#include <functional>
#include <fstream>
#include "CubemapUnwrapUtils.h"

using namespace vtd;

DEFINE_LOG_CATEGORY(SceneExporter);
#define LOCTEXT_NAMESPACE "FSceneExporterModule"

UExporter* GetFBXExporter()
{
	TArray<UExporter*> aryExporters;
	ObjectTools::AssembleListOfExporters(aryExporters);
	for (auto pkExporter : aryExporters)
	{
		if (pkExporter->GetClass()->GetName() == "StaticMeshExporterFBX")
		{
			return pkExporter;
		}
	}
	return nullptr;
}

UExporter* GetTGAExporter()
{
	TArray<UExporter*> aryExporters;
	ObjectTools::AssembleListOfExporters(aryExporters);
	for (auto pkExporter : aryExporters)
	{
		if (pkExporter->GetClass()->GetName() == "TextureExporterTGA")
		{
			return pkExporter;
		}
	}
	return nullptr;
}

int8 char_num(TCHAR c)
{
	switch (c)
	{
	case '0':
		return 0;
	case '1':
		return 1;
	case '2':
		return 2;
	case '3':
		return 3;
	case '4':
		return 4;
	case '5':
		return 5;
	case '6':
		return 6;
	case '7':
		return 7;
	case '8':
		return 8;
	case '9':
		return 9;
	case 'A':
		return 10;
	case 'B':
		return 11;
	case 'C':
		return 12;
	case 'D':
		return 13;
	case 'E':
		return 14;
	case 'F':
		return 15;
	default:
		break;
	}
	return 0;
}

TCHAR num_char(int8 n)
{
	switch (n)
	{
	case 0:
		return TEXT('0');
	case 1:
		return TEXT('1');
	case 2:
		return TEXT('2');
	case 3:
		return TEXT('3');
	case 4:
		return TEXT('4');
	case 5:
		return TEXT('5');
	case 6:
		return TEXT('6');
	case 7:
		return TEXT('7');
	case 8:
		return TEXT('8');
	case 9:
		return TEXT('9');
	case 10:
		return TEXT('A');
	case 11:
		return TEXT('B');
	case 12:
		return TEXT('C');
	case 13:
		return TEXT('D');
	case 14:
		return TEXT('E');
	case 15:
		return TEXT('F');
	default:
		break;
	}
	return TEXT('0');
}

uint32 po2(uint32 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

uint32 roundpos(float v, uint32 w)
{
	return (uint32)(FPlatformMath::RoundToInt(v / float(w))) * w;
}

class CTextureCubeWrite
{
public:
	CTextureCubeWrite(const TCHAR *fileName)
	{
		m_file.open(fileName, std::ios::out | std::ios::trunc);
	}
	~CTextureCubeWrite()
	{
		if (m_file)
			m_file.close();
	}

	static FColor ToRGBEDithered(const FLinearColor& ColorIN, const FRandomStream& Rand)
	{
		const float R = ColorIN.R;
		const float G = ColorIN.G;
		const float B = ColorIN.B;
		const float Primary = FMath::Max3(R, G, B);
		FColor	ReturnColor;

		if (Primary < 1E-32)
		{
			ReturnColor = FColor(0, 0, 0, 0);
		}
		else
		{
			int32 Exponent;
			const float Scale = frexp(Primary, &Exponent) / Primary * 255.f;

			ReturnColor.R = FMath::Clamp(FMath::TruncToInt((R* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.G = FMath::Clamp(FMath::TruncToInt((G* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.B = FMath::Clamp(FMath::TruncToInt((B* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.A = FMath::Clamp(FMath::TruncToInt(Exponent), -128, 127) + 128;
		}

		return ReturnColor;
	}

	template< typename type>
	bool Write(type writedata)
	{
		if (m_file)
		{
			m_file << writedata << std::endl;
			return true;
		}
		return false;
	}
	bool WriteTexture(const FTextureResource* TextureResource, int sizex, EPixelFormat informat)
	{
		TArray<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TextureResource, sizex, informat, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData);
		return true;
	}

	template<typename TSourceColorType>
	void WriteHDRBits(TSourceColorType* SourceTexels)
	{
		const FRandomStream RandomStream(0xA1A1);
		const int32 NumChannels = 4;
		const int32 SizeX = Size.X;
		const int32 SizeY = Size.Y;
		TArray<uint8> ScanLine[NumChannels];
		for (int32 Channel = 0; Channel < NumChannels; Channel++)
		{
			ScanLine[Channel].Reserve(SizeX);
		}

		for (int32 y = 0; y < SizeY; y++)
		{
			// write RLE header
			uint8 RLEheader[4];
			RLEheader[0] = 2;
			RLEheader[1] = 2;
			RLEheader[2] = SizeX >> 8;
			RLEheader[3] = SizeX & 0xFF;
			//Ar.Serialize(&RLEheader[0], sizeof(RLEheader));
			m_file.write((char*)&RLEheader[0], sizeof(RLEheader));

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				ScanLine[Channel].Reset();
			}

			for (int32 x = 0; x < SizeX; x++)
			{
				FLinearColor LinearColor(*SourceTexels);
				FColor RGBEColor = ToRGBEDithered(LinearColor, RandomStream);

				FLinearColor lintest = RGBEColor.FromRGBE();
				ScanLine[0].Add(RGBEColor.R);
				ScanLine[1].Add(RGBEColor.G);
				ScanLine[2].Add(RGBEColor.B);
				ScanLine[3].Add(RGBEColor.A);
				SourceTexels++;
			}

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				WriteScanLine(ScanLine[Channel]);
			}
		}
	}

	void WriteScanLine(const TArray<uint8>& ScanLine)
	{
		const uint8* LineEnd = ScanLine.GetData() + ScanLine.Num();
		const uint8* LineSource = ScanLine.GetData();
		TArray<uint8> Output;
		Output.Reserve(ScanLine.Num() * 2);
		while (LineSource < LineEnd)
		{
			int32 CurrentPos = 0;
			int32 NextPos = 0;
			int32 CurrentRunLength = 0;
			while (CurrentRunLength <= 4 && NextPos < 128 && LineSource + NextPos < LineEnd)
			{
				CurrentPos = NextPos;
				CurrentRunLength = 0;
				while (CurrentRunLength < 127 && CurrentPos + CurrentRunLength < 128 && LineSource + NextPos < LineEnd && LineSource[CurrentPos] == LineSource[NextPos])
				{
					NextPos++;
					CurrentRunLength++;
				}
			}

			if (CurrentRunLength > 4)
			{
				// write a non run: LineSource[0] to LineSource[CurrentPos]
				if (CurrentPos > 0)
				{
					Output.Add(CurrentPos);
					for (int32 i = 0; i < CurrentPos; i++)
					{
						Output.Add(LineSource[i]);
					}
				}
				Output.Add((uint8)(128 + CurrentRunLength));
				Output.Add(LineSource[CurrentPos]);
			}
			else
			{
				// write a non run: LineSource[0] to LineSource[NextPos]
				Output.Add((uint8)(NextPos));
				for (int32 i = 0; i < NextPos; i++)
				{
					Output.Add((uint8)(LineSource[i]));
				}
			}
			LineSource += NextPos;
		}
		//Ar.Serialize(Output.GetData(), Output.Num());
		m_file.write((char*)Output.GetData(), Output.Num());
	}

	void WriteHDRImage(const TArray<uint8>& RawData)
	{
		WriteHDRHeader();
		if (Format == PF_FloatRGBA)
		{
			WriteHDRBits((FFloat16Color*)RawData.GetData());
		}
		else
		{
			WriteHDRBits((FColor*)RawData.GetData());
		}
	}

	void WriteHDRHeader()
	{
		const int32 MaxHeaderSize = 256;
		char Header[MAX_SPRINTF];
		FCStringAnsi::Sprintf(Header, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", Size.Y, Size.X);
		Header[MaxHeaderSize - 1] = 0;
		int32 Len = FMath::Min(FCStringAnsi::Strlen(Header), MaxHeaderSize);
		m_file.write(Header, Len);
	}

	FIntPoint Size;
	EPixelFormat Format;
private:
	std::ofstream m_file;
};

#define DDSD_CAPS  0x1
#define DDSD_HEIGHT  0x2
#define DDSD_WIDTH  0x4
#define DDSD_PITCH  0x8
#define DDSD_PIXELFORMAT  0x1000
#define DDSD_MIPMAPCOUNT  0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_ALPHA       0x00000002
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB    0x00000040
#define DDSF_RGBA  0x00000041
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000

#define DDSCAPS2_CUBEMAP 0x200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x1000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x2000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x4000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x8000
#define DDSCAPS2_CUBEMAP_ALL_FACES 0x0000FC00
#define DDSCAPS2_VOLUME 0x200000

// dwCaps2 flags
const uint32 DDSF_CUBEMAP = 0x00000200;
const uint32 DDSF_CUBEMAP_POSITIVEX = 0x00000400;
const uint32 DDSF_CUBEMAP_NEGATIVEX = 0x00000800;
const uint32 DDSF_CUBEMAP_POSITIVEY = 0x00001000;
const uint32 DDSF_CUBEMAP_NEGATIVEY = 0x00002000;
const uint32 DDSF_CUBEMAP_POSITIVEZ = 0x00004000;
const uint32 DDSF_CUBEMAP_NEGATIVEZ = 0x00008000;
const uint32 DDSF_CUBEMAP_ALL_FACES = 0x0000FC00;
const uint32 DDSF_VOLUME = 0x00200000;

#define GL_BGR_EXT                                        0x80E0
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3

#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_BGR_EXT                        0x80E0
#define GL_BGRA_EXT                       0x80E1

// compressed texture types
const uint32_t FOURCC_DXT1 = 0x31545844; //(MAKEFOURCC('D','X','T','1'))
const uint32_t FOURCC_DXT3 = 0x33545844; //(MAKEFOURCC('D','X','T','3'))
const uint32_t FOURCC_DXT5 = 0x35545844; //(MAKEFOURCC('D','X','T','5'))

FString fourcc(uint32_t enc) {
	char c[5] = { '\0' };
	c[0] = enc >> 0 & 0xFF;
	c[1] = enc >> 8 & 0xFF;
	c[2] = enc >> 16 & 0xFF;
	c[3] = enc >> 24 & 0xFF;
	return c;
}

struct DXTColBlock {
	uint16_t col0;
	uint16_t col1;

	uint8_t row[4];
};

struct DXT3AlphaBlock {
	uint16_t row[4];
};

struct DXT5AlphaBlock {
	uint8_t alpha0;
	uint8_t alpha1;

	uint8_t row[6];
};

///////////////////////////////////////////////////////////////////////////////
// flip a DXT1 color block
void flip_blocks_dxtc1(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;

	for (unsigned int i = 0; i < numBlocks; i++) {
		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT3 color block
void flip_blocks_dxtc3(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;
	DXT3AlphaBlock *alphablock;

	for (unsigned int i = 0; i < numBlocks; i++) {
		alphablock = (DXT3AlphaBlock*)curblock;

		std::swap(alphablock->row[0], alphablock->row[3]);
		std::swap(alphablock->row[1], alphablock->row[2]);

		curblock++;

		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT5 alpha block
void flip_dxt5_alpha(DXT5AlphaBlock *block) {
	uint8_t gBits[4][4];

	const uint32_t mask = 0x00000007;          // bits = 00 00 01 11
	uint32_t bits = 0;
	memcpy(&bits, &block->row[0], sizeof(uint8_t) * 3);

	gBits[0][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[0][3] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[1][3] = (uint8_t)(bits & mask);

	bits = 0;
	memcpy(&bits, &block->row[3], sizeof(uint8_t) * 3);

	gBits[2][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[2][3] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][0] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][1] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][2] = (uint8_t)(bits & mask);
	bits >>= 3;
	gBits[3][3] = (uint8_t)(bits & mask);

	uint32_t *pBits = ((uint32_t*) &(block->row[0]));

	*pBits = *pBits | (gBits[3][0] << 0);
	*pBits = *pBits | (gBits[3][1] << 3);
	*pBits = *pBits | (gBits[3][2] << 6);
	*pBits = *pBits | (gBits[3][3] << 9);

	*pBits = *pBits | (gBits[2][0] << 12);
	*pBits = *pBits | (gBits[2][1] << 15);
	*pBits = *pBits | (gBits[2][2] << 18);
	*pBits = *pBits | (gBits[2][3] << 21);

	pBits = ((uint32_t*) &(block->row[3]));

#ifdef MACOS
	*pBits &= 0x000000ff;
#else
	*pBits &= 0xff000000;
#endif

	*pBits = *pBits | (gBits[1][0] << 0);
	*pBits = *pBits | (gBits[1][1] << 3);
	*pBits = *pBits | (gBits[1][2] << 6);
	*pBits = *pBits | (gBits[1][3] << 9);

	*pBits = *pBits | (gBits[0][0] << 12);
	*pBits = *pBits | (gBits[0][1] << 15);
	*pBits = *pBits | (gBits[0][2] << 18);
	*pBits = *pBits | (gBits[0][3] << 21);
}

///////////////////////////////////////////////////////////////////////////////
// flip a DXT5 color block
void flip_blocks_dxtc5(DXTColBlock *line, unsigned int numBlocks) {
	DXTColBlock *curblock = line;
	DXT5AlphaBlock *alphablock;

	for (unsigned int i = 0; i < numBlocks; i++) {
		alphablock = (DXT5AlphaBlock*)curblock;

		flip_dxt5_alpha(alphablock);

		curblock++;

		std::swap(curblock->row[0], curblock->row[3]);
		std::swap(curblock->row[1], curblock->row[2]);

		curblock++;
	}
}



typedef struct {
	uint32 dwSize;
	uint32 dwFlags;
	uint32 dwFourCC;
	uint32 dwRGBBitCount;
	uint32 dwRBitMask;
	uint32 dwGBitMask;
	uint32 dwBBitMask;
	uint32 dwABitMask;
} DDS_PIXELFORMAT;

struct DDS_HEADER
{
	uint32           dwSize;
	uint32           dwFlags;
	uint32           dwHeight;
	uint32           dwWidth;
	uint32           dwLinearSize;
	uint32           dwDepth;
	uint32           dwMipMapCount;
	uint32           dwReserved1[11];
	DDS_PIXELFORMAT ddpf;
	uint32           dwCaps;
	uint32           dwCaps2;
	uint32           dwCaps3;
	uint32           dwCaps4;
	uint32           dwReserved2;
};

enum TextureType {
	TextureNone, TextureFlat,    // 1D, 2D textures
	Texture3D,
	TextureCubemap
};
class CSurface {
public:
	CSurface();
	CSurface(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels);
	CSurface(const CSurface &copy);
	CSurface &operator=(const CSurface &rhs);
	virtual ~CSurface();

	operator uint8*() const;

	virtual void create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels);
	virtual void clear();

	unsigned int get_width() const {
		return m_width;
	}
	unsigned int get_height() const {
		return m_height;
	}
	unsigned int get_depth() const {
		return m_depth;
	}
	unsigned int get_size() const {
		return m_size;
	}

	friend class CTexture;

private:
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_depth;
	unsigned int m_size;

	uint8 *m_pixels;
};

//////////////////////////////////////////////////////////////////////////////
// CSurface implementation
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// default constructor
CSurface::CSurface() :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty image
CSurface::CSurface(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
	create(w, h, d, imgsize, pixels);
}

///////////////////////////////////////////////////////////////////////////////
// copy constructor
CSurface::CSurface(const CSurface &copy) :
	m_width(0), m_height(0), m_depth(0), m_size(0), m_pixels(NULL) {
	if (copy.get_size() != 0) {
		m_size = copy.get_size();
		m_width = copy.get_width();
		m_height = copy.get_height();
		m_depth = copy.get_depth();

		m_pixels = new uint8_t[m_size];
		memcpy(m_pixels, copy, m_size);
	}
}

///////////////////////////////////////////////////////////////////////////////
// assignment operator
CSurface &CSurface::operator=(const CSurface &rhs) {
	if (this != &rhs) {
		clear();

		if (rhs.get_size()) {
			m_size = rhs.get_size();
			m_width = rhs.get_width();
			m_height = rhs.get_height();
			m_depth = rhs.get_depth();

			m_pixels = new uint8[m_size];
			memcpy(m_pixels, rhs, m_size);
		}
	}

	return *this;
}

///////////////////////////////////////////////////////////////////////////////
// clean up image memory
CSurface::~CSurface() {
	clear();
}

///////////////////////////////////////////////////////////////////////////////
// returns a pointer to image
CSurface::operator uint8_t*() const {
	return m_pixels;
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty image
void CSurface::create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) {

	clear();

	m_width = w;
	m_height = h;
	m_depth = d;
	m_size = imgsize;
	m_pixels = new uint8_t[imgsize];
	memcpy(m_pixels, pixels, imgsize);
}

///////////////////////////////////////////////////////////////////////////////
// free surface memory
void CSurface::clear() {
	if (m_pixels != NULL) {
		delete[] m_pixels;
		m_pixels = NULL;
	}
}

class CTexture : public CSurface {
	friend class CDDSImage;

public:
	CTexture();
	CTexture(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8_t *pixels);
	CTexture(const CTexture &copy);
	CTexture &operator=(const CTexture &rhs);
	~CTexture();

	void create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8_t *pixels);
	void clear();

	const CSurface &get_mipmap(unsigned int index) const {

		return m_mipmaps[index];
	}

	void add_mipmap(const CSurface &mipmap) {
		m_mipmaps.Push(mipmap);
	}

	unsigned int get_num_mipmaps() const {
		return (unsigned int)m_mipmaps.Num();
	}

	void FlipX()
	{
		uint32* buffer = (uint32*)m_pixels;
		for (uint32 i(0); i < m_height; ++i)
		{
			for (uint32 j(0); j < (m_width >> 1); ++j)
			{
				uint32 temp = buffer[i * m_width + j];
				buffer[i * m_width + j] = buffer[i * m_width + m_width - j - 1];
				buffer[i * m_width + m_width - j - 1] = temp;
			}
		}

		for (auto& sur : m_mipmaps)
		{
			buffer = (uint32*)sur.m_pixels;
			for (uint32 i(0); i < sur.m_height; ++i)
			{
				for (uint32 j(0); j < (sur.m_width >> 1); ++j)
				{
					uint32 temp = buffer[i * sur.m_width + j];
					buffer[i * sur.m_width + j] = buffer[i * sur.m_width + sur.m_width - j - 1];
					buffer[i * sur.m_width + sur.m_width - j - 1] = temp;
				}
			}
		}
	}

protected:
	CSurface &get_mipmap(unsigned int index) {

		return m_mipmaps[index];
	}

private:
	TArray<CSurface> m_mipmaps;
};

///////////////////////////////////////////////////////////////////////////////
// CTexture implementation
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// default constructor
CTexture::CTexture() :
	CSurface()  // initialize base class part
{
}

///////////////////////////////////////////////////////////////////////////////
// creates an empty texture
CTexture::CTexture(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) :
	CSurface(w, h, d, imgsize, pixels)  // initialize base class part
{
}

CTexture::~CTexture() {
}

///////////////////////////////////////////////////////////////////////////////
// copy constructor
CTexture::CTexture(const CTexture &copy) :
	CSurface(copy) {
	for (unsigned int i = 0; i < copy.get_num_mipmaps(); i++)
		m_mipmaps.Push(copy.get_mipmap(i));
}

///////////////////////////////////////////////////////////////////////////////
// assignment operator
CTexture &CTexture::operator=(const CTexture &rhs) {
	if (this != &rhs) {
		CSurface::operator =(rhs);

		m_mipmaps.Empty();
		for (unsigned int i = 0; i < rhs.get_num_mipmaps(); i++)
			m_mipmaps.Push(rhs.get_mipmap(i));
	}

	return *this;
}

void CTexture::create(unsigned int w, unsigned int h, unsigned int d, unsigned int imgsize, const uint8 *pixels) {
	CSurface::create(w, h, d, imgsize, pixels);

	m_mipmaps.Empty();
}

void CTexture::clear() {
	CSurface::clear();

	m_mipmaps.Empty();
}


class CDDSImage {
public:
	CDDSImage();
	~CDDSImage();

	void create_textureFlat(unsigned int format, unsigned int components, const CTexture &baseImage);
	void create_texture3D(unsigned int format, unsigned int components, const CTexture &baseImage);
	void create_textureCubemap(unsigned int format, unsigned int components, const CTexture &positiveX, const CTexture &negativeX, const CTexture &positiveY,
		const CTexture &negativeY, const CTexture &positiveZ, const CTexture &negativeZ);

	void clear();

	void load(std::istream& is, bool flipImage = true);
	void load(const FString& filename, bool flipImage = true);
	void save(const FString& filename, bool flipImage = true);


	operator uint8_t*() {

		return m_images[0];
	}

	unsigned int get_width() {
		return m_images[0].get_width();
	}

	unsigned int get_height() {
		return m_images[0].get_height();
	}

	unsigned int get_depth() {
		return m_images[0].get_depth();
	}

	unsigned int get_size() {
		return m_images[0].get_size();
	}

	unsigned int get_num_mipmaps() {
		return m_images[0].get_num_mipmaps();
	}

	const CSurface &get_mipmap(unsigned int index) const {
		return m_images[0].get_mipmap(index);
	}

	const CTexture &get_cubemap_face(unsigned int face) const {
		return m_images[face];
	}

	unsigned int get_components() {
		return m_components;
	}
	unsigned int get_format() {
		return m_format;
	}
	TextureType get_type() {
		return m_type;
	}

	bool is_compressed();

	bool is_cubemap() {
		return (m_type == TextureCubemap);
	}
	bool is_volume() {
		return (m_type == Texture3D);
	}
	bool is_valid() {
		return m_valid;
	}

	bool is_dword_aligned() {

		int dwordLineSize = get_dword_aligned_linesize(get_width(), m_components * 8);
		int curLineSize = get_width() * m_components;

		return (dwordLineSize == curLineSize);
	}

private:
	unsigned int clamp_size(unsigned int size);
	unsigned int size_dxtc(unsigned int width, unsigned int height);
	unsigned int size_rgb(unsigned int width, unsigned int height);

	// calculates 4-byte aligned width of image
	unsigned int get_dword_aligned_linesize(unsigned int width, unsigned int bpp) {
		return ((width * bpp + 31) & -32) >> 3;
	}

	void flip(CSurface &surface);
	void flip_texture(CTexture &texture);

	void write_texture(const CTexture &texture, std::ostream& os);

	unsigned int m_format;
	unsigned int m_components;
	TextureType m_type;
	bool m_valid;

	TArray<CTexture> m_images;
};

///////////////////////////////////////////////////////////////////////////////
// CDDSImage public functions

///////////////////////////////////////////////////////////////////////////////
// default constructor
CDDSImage::CDDSImage() :
	m_format(0), m_components(0), m_type(TextureNone), m_valid(false) {
}

CDDSImage::~CDDSImage() {
}

void CDDSImage::create_textureFlat(unsigned int format, unsigned int components, const CTexture &baseImage) {

	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = TextureFlat;

	m_images.Push(baseImage);

	m_valid = true;
}

void CDDSImage::create_texture3D(unsigned int format, unsigned int components, const CTexture &baseImage) {

	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = Texture3D;

	m_images.Push(baseImage);

	m_valid = true;
}

inline bool same_size(const CTexture &a, const CTexture &b) {
	if (a.get_width() != b.get_width())
		return false;
	if (a.get_height() != b.get_height())
		return false;
	if (a.get_depth() != b.get_depth())
		return false;

	return true;
}

void CDDSImage::create_textureCubemap(unsigned int format, unsigned int components, const CTexture &positiveX, const CTexture &negativeX,
	const CTexture &positiveY, const CTexture &negativeY, const CTexture &positiveZ, const CTexture &negativeZ) {



	// remove any existing images
	clear();

	m_format = format;
	m_components = components;
	m_type = TextureCubemap;

	m_images.Push(positiveX);
	m_images.Push(negativeX);
	m_images.Push(positiveY);
	m_images.Push(negativeY);
	m_images.Push(positiveZ);
	m_images.Push(negativeZ);

	m_valid = true;
}

///////////////////////////////////////////////////////////////////////////////
// loads DDS image
//
// filename - fully qualified name of DDS image
// flipImage - specifies whether image is flipped on load, default is true
void CDDSImage::load(const FString& filename, bool flipImage) {

	std::ifstream fs(*filename, std::ios::binary);
	load(fs, flipImage);
}

///////////////////////////////////////////////////////////////////////////////
// loads DDS image
//
// is - istream to read the image from
// flipImage - specifies whether image is flipped on load, default is true
void CDDSImage::load(std::istream& is, bool flipImage) {
	// clear any previously loaded images
	clear();

	// read in file marker, make sure its a DDS file
	char filecode[4];
	is.read(filecode, 4);
	if (strncmp(filecode, "DDS ", 4) != 0) {
		return;
	}

	// read in DDS header
	DDS_HEADER ddsh;
	is.read((char*)&ddsh, sizeof(DDS_HEADER));

	// default to flat texture type (1D, 2D, or rectangle)
	m_type = TextureFlat;

	// check if image is a cubemap
	if (ddsh.dwCaps2 & DDPF_FOURCC)
		m_type = TextureCubemap;

	// check if image is a volume texture
	if ((ddsh.dwCaps2 & DDSF_VOLUME) && (ddsh.dwDepth > 0))
		m_type = Texture3D;

	// figure out what the image format is
	if (ddsh.ddpf.dwFlags & DDPF_FOURCC) {
		switch (ddsh.ddpf.dwFourCC) {
		case FOURCC_DXT1:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			m_components = 3;
			break;
		case FOURCC_DXT3:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			m_components = 4;
			break;
		case FOURCC_DXT5:
			m_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			m_components = 4;
			break;
		default:
			break;
			//throw runtime_error("unknown texture compression '" + fourcc(ddsh.ddspf.dwFourCC) + "'");
		}
	}
	else if (ddsh.ddpf.dwRGBBitCount == 32 &&
		ddsh.ddpf.dwRBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x000000FF &&
		ddsh.ddpf.dwABitMask == 0xFF000000) {
		m_format = GL_BGRA_EXT;
		m_components = 4;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 32 &&
		ddsh.ddpf.dwRBitMask == 0x000000FF &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwABitMask == 0xFF000000) {
		m_format = GL_RGBA;
		m_components = 4;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 24 &&
		ddsh.ddpf.dwRBitMask == 0x000000FF &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x00FF0000) {
		m_format = GL_RGB;
		m_components = 3;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 24 &&
		ddsh.ddpf.dwRBitMask == 0x00FF0000 &&
		ddsh.ddpf.dwGBitMask == 0x0000FF00 &&
		ddsh.ddpf.dwBBitMask == 0x000000FF) {
		m_format = GL_BGR_EXT;
		m_components = 3;
	}
	else if (ddsh.ddpf.dwRGBBitCount == 8) {
		m_format = GL_LUMINANCE;
		m_components = 1;
	}
	else {
		//throw runtime_error("unknow texture format");
	}

	// store primary surface width/height/depth
	unsigned int width, height, depth;
	width = ddsh.dwWidth;
	height = ddsh.dwHeight;
	depth = clamp_size(ddsh.dwDepth);   // set to 1 if 0

										// use correct size calculation function depending on whether image is
										// compressed
	unsigned int (CDDSImage::*sizefunc)(unsigned int, unsigned int);
	sizefunc = (is_compressed() ? &CDDSImage::size_dxtc : &CDDSImage::size_rgb);

	// load all surfaces for the image (6 surfaces for cubemaps)
	for (unsigned int n = 0; n < (unsigned int)(m_type == TextureCubemap ? 6 : 1); n++) {
		// add empty texture object
		m_images.Push(CTexture());

		// get reference to newly added texture object
		CTexture &img = m_images[n];

		// calculate surface size
		unsigned int size = (this->*sizefunc)(width, height) * depth;

		// load surface
		uint8_t *pixels = new uint8_t[size];
		is.read((char*)pixels, size);

		img.create(width, height, depth, size, pixels);

		delete[] pixels;

		if (flipImage)
			flip(img);

		unsigned int w = clamp_size(width >> 1);
		unsigned int h = clamp_size(height >> 1);
		unsigned int d = clamp_size(depth >> 1);

		// store number of mipmaps
		unsigned int numMipmaps = ddsh.dwMipMapCount;

		// number of mipmaps in file includes main surface so decrease count
		// by one
		if (numMipmaps != 0)
			numMipmaps--;

		// load all mipmaps for current surface
		for (unsigned int i = 0; i < numMipmaps && (w || h); i++) {
			// add empty surface
			img.add_mipmap(CSurface());

			// get reference to newly added mipmap
			CSurface &mipmap = img.get_mipmap(i);

			// calculate mipmap size
			size = (this->*sizefunc)(w, h) * d;

			uint8_t *pixels = new uint8_t[size];
			is.read((char*)pixels, size);

			mipmap.create(w, h, d, size, pixels);

			delete[] pixels;

			if (flipImage)
				flip(mipmap);

			// shrink to next power of 2
			w = clamp_size(w >> 1);
			h = clamp_size(h >> 1);
			d = clamp_size(d >> 1);
		}
	}

	// swap cubemaps on y axis (since image is flipped in OGL)
	if (m_type == TextureCubemap && flipImage) {
		CTexture tmp;
		tmp = m_images[3];
		m_images[3] = m_images[2];
		m_images[2] = tmp;
	}

	m_valid = true;
}

void CDDSImage::write_texture(const CTexture &texture, std::ostream& os) {

	os.write((char*)(uint8_t*)texture, texture.get_size());

	for (unsigned int i = 0; i < texture.get_num_mipmaps(); i++) {
		const CSurface &mipmap = texture.get_mipmap(i);
		os.write((char*)(uint8_t*)mipmap, mipmap.get_size());
	}
}

void CDDSImage::save(const FString& filename, bool flipImage) {


	DDS_HEADER ddsh;
	unsigned int headerSize = sizeof(DDS_HEADER);
	memset(&ddsh, 0, headerSize);
	ddsh.dwSize = headerSize;
	ddsh.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	ddsh.dwHeight = get_height();
	ddsh.dwWidth = get_width();

	if (is_compressed()) {
		ddsh.dwFlags |= DDSD_LINEARSIZE;
		ddsh.dwLinearSize = get_size();
	}
	else {
		ddsh.dwFlags |= DDSD_PITCH;
		ddsh.dwLinearSize = get_dword_aligned_linesize(get_width(), m_components * 8);
	}

	if (m_type == Texture3D) {
		ddsh.dwFlags |= DDSD_DEPTH;
		ddsh.dwDepth = get_depth();
	}

	if (get_num_mipmaps() > 0) {
		ddsh.dwFlags |= DDSD_MIPMAPCOUNT;
		ddsh.dwMipMapCount = get_num_mipmaps() + 1;
	}

	ddsh.ddpf.dwSize = sizeof(DDS_PIXELFORMAT);

	if (is_compressed()) {
		ddsh.ddpf.dwFlags = DDPF_FOURCC;

		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT1;
		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT3;
		if (m_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
			ddsh.ddpf.dwFourCC = FOURCC_DXT5;
	}
	else {
		ddsh.ddpf.dwFlags = (m_components == 4) ? DDSF_RGBA : DDPF_RGB;
		ddsh.ddpf.dwRGBBitCount = m_components * 8;
		ddsh.ddpf.dwRBitMask = 0x00ff0000;
		ddsh.ddpf.dwGBitMask = 0x0000ff00;
		ddsh.ddpf.dwBBitMask = 0x000000ff;

		if (m_components == 4) {
			ddsh.ddpf.dwFlags |= DDPF_ALPHAPIXELS;
			ddsh.ddpf.dwABitMask = 0xff000000;
		}
	}

	ddsh.dwCaps = DDSCAPS_TEXTURE;

	if (m_type == TextureCubemap) {
		ddsh.dwCaps |= DDSCAPS_COMPLEX;
		ddsh.dwCaps2 = DDSF_CUBEMAP | DDSF_CUBEMAP_ALL_FACES;
	}

	if (m_type == Texture3D) {
		ddsh.dwCaps |= DDSCAPS_COMPLEX;
		ddsh.dwCaps2 = DDSF_VOLUME;
	}

	if (get_num_mipmaps() > 0)
		ddsh.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

	// open file
	std::ofstream of;
	of.exceptions(std::ios::failbit);
	of.open(*filename, std::ios::binary);

	// write file header
	of.write("DDS ", 4);

	// write dds header
	of.write((char*)&ddsh, sizeof(DDS_HEADER));

	if (m_type != TextureCubemap) {
		CTexture tex = m_images[0];
		if (flipImage)
			flip_texture(tex);
		write_texture(tex, of);
	}
	else {


		for (int i = 0; i < m_images.Num(); i++) {
			CTexture cubeFace;

			if (i == 2)
				cubeFace = m_images[3];
			else if (i == 3)
				cubeFace = m_images[2];
			else
				cubeFace = m_images[i];

			if (flipImage)
				flip_texture(cubeFace);
			write_texture(cubeFace, of);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// free image memory
void CDDSImage::clear() {
	m_components = 0;
	m_format = 0;
	m_type = TextureNone;
	m_valid = false;

	m_images.Empty();
}


bool CDDSImage::is_compressed() {
	return (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
		|| (m_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
		|| (m_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
}

///////////////////////////////////////////////////////////////////////////////
// clamps input size to [1-size]
inline unsigned int CDDSImage::clamp_size(unsigned int size) {
	if (size <= 0)
		size = 1;

	return size;
}

///////////////////////////////////////////////////////////////////////////////
// CDDSImage private functions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// calculates size of DXTC texture in bytes
inline unsigned int CDDSImage::size_dxtc(unsigned int width, unsigned int height) {
	return ((width + 3) / 4) * ((height + 3) / 4) * (m_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16);
}

///////////////////////////////////////////////////////////////////////////////
// calculates size of uncompressed RGB texture in bytes
inline unsigned int CDDSImage::size_rgb(unsigned int width, unsigned int height) {
	return width * height * m_components;
}

///////////////////////////////////////////////////////////////////////////////
// flip image around X axis
void CDDSImage::flip(CSurface &surface) {
	unsigned int linesize;
	unsigned int offset;

	if (!is_compressed()) {

		unsigned int imagesize = surface.get_size() / surface.get_depth();
		linesize = imagesize / surface.get_height();

		uint8_t *tmp = new uint8_t[linesize];

		for (unsigned int n = 0; n < surface.get_depth(); n++) {
			offset = imagesize * n;
			uint8_t *top = (uint8_t*)surface + offset;
			uint8_t *bottom = top + (imagesize - linesize);

			for (unsigned int i = 0; i < (surface.get_height() >> 1); i++) {
				// swap
				memcpy(tmp, bottom, linesize);
				memcpy(bottom, top, linesize);
				memcpy(top, tmp, linesize);

				top += linesize;
				bottom -= linesize;
			}
		}

		delete[] tmp;
	}
	else {
		void(*flipblocks)(DXTColBlock*, unsigned int);
		unsigned int xblocks = surface.get_width() / 4;
		unsigned int yblocks = surface.get_height() / 4;
		unsigned int blocksize;

		switch (m_format) {
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			blocksize = 8;
			flipblocks = flip_blocks_dxtc1;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			blocksize = 16;
			flipblocks = flip_blocks_dxtc3;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			blocksize = 16;
			flipblocks = flip_blocks_dxtc5;
			break;
		default:
			return;
		}

		linesize = xblocks * blocksize;

		DXTColBlock *top;
		DXTColBlock *bottom;

		uint8_t *tmp = new uint8_t[linesize];

		for (unsigned int j = 0; j < (yblocks >> 1); j++) {
			top = (DXTColBlock*)((uint8_t*)surface + j * linesize);
			bottom = (DXTColBlock*)((uint8_t*)surface + (((yblocks - j) - 1) * linesize));

			flipblocks(top, xblocks);
			flipblocks(bottom, xblocks);

			// swap
			memcpy(tmp, bottom, linesize);
			memcpy(bottom, top, linesize);
			memcpy(top, tmp, linesize);
		}

		delete[] tmp;
	}
}

void CDDSImage::flip_texture(CTexture &texture) {
	flip(texture);

	for (unsigned int i = 0; i < texture.get_num_mipmaps(); i++) {
		flip(texture.get_mipmap(i));
	}
}


class LightMap2DExt : public FLightMap2D
{
public:
	FVector4 GetScaleVector(uint32 i) { return ScaleVectors[i]; }
	FVector4 GetAddVector(uint32 i) { return AddVectors[i]; }

};

#define FACE_X_POS 0
#define FACE_X_NEG 1
#define FACE_Y_POS 2
#define FACE_Y_NEG 3
#define FACE_Z_POS 4
#define FACE_Z_NEG 5

#define EDGE_LEFT   0	 // u = 0
#define EDGE_RIGHT  1	 // u = 1
#define EDGE_TOP    2	 // v = 0
#define EDGE_BOTTOM 3	 // v = 1

#define CORNER_NNN  0
#define CORNER_NNP  1
#define CORNER_NPN  2
#define CORNER_NPP  3
#define CORNER_PNN  4
#define CORNER_PNP  5
#define CORNER_PPN  6
#define CORNER_PPP  7

static const int32 CubeEdgeListA[12][2] =
{
	{ FACE_X_POS, EDGE_LEFT },
	{ FACE_X_POS, EDGE_RIGHT },
	{ FACE_X_POS, EDGE_TOP },
	{ FACE_X_POS, EDGE_BOTTOM },

	{ FACE_X_NEG, EDGE_LEFT },
	{ FACE_X_NEG, EDGE_RIGHT },
	{ FACE_X_NEG, EDGE_TOP },
	{ FACE_X_NEG, EDGE_BOTTOM },

	{ FACE_Z_POS, EDGE_TOP },
	{ FACE_Z_POS, EDGE_BOTTOM },
	{ FACE_Z_NEG, EDGE_TOP },
	{ FACE_Z_NEG, EDGE_BOTTOM }
};

static const int32 CubeEdgeListB[12][2] =
{
	{ FACE_Z_POS, EDGE_RIGHT },
	{ FACE_Z_NEG, EDGE_LEFT },
	{ FACE_Y_POS, EDGE_RIGHT },
	{ FACE_Y_NEG, EDGE_RIGHT },

	{ FACE_Z_NEG, EDGE_RIGHT },
	{ FACE_Z_POS, EDGE_LEFT },
	{ FACE_Y_POS, EDGE_LEFT },
	{ FACE_Y_NEG, EDGE_LEFT },

	{ FACE_Y_POS, EDGE_BOTTOM },
	{ FACE_Y_NEG, EDGE_TOP },
	{ FACE_Y_POS, EDGE_TOP },
	{ FACE_Y_NEG, EDGE_BOTTOM },
};

// Index by [Face][Corner]
static const int32 CubeCornerList[6][4] =
{
	{ CORNER_PPP, CORNER_PPN, CORNER_PNP, CORNER_PNN },
	{ CORNER_NPN, CORNER_NPP, CORNER_NNN, CORNER_NNP },
	{ CORNER_NPN, CORNER_PPN, CORNER_NPP, CORNER_PPP },
	{ CORNER_NNP, CORNER_PNP, CORNER_NNN, CORNER_PNN },
	{ CORNER_NPP, CORNER_PPP, CORNER_NNP, CORNER_PNP },
	{ CORNER_PPN, CORNER_NPN, CORNER_PNN, CORNER_NNN }
};

FColor RGBMEncode(FLinearColor Color)
{
	FColor Encoded;

	// Convert to gamma space
	Color.R = FMath::Sqrt(Color.R);
	Color.G = FMath::Sqrt(Color.G);
	Color.B = FMath::Sqrt(Color.B);

	// Range
	Color /= 16.0f;

	float MaxValue = FMath::Max(FMath::Max(Color.R, Color.G), FMath::Max(Color.B, DELTA));

	if (MaxValue > 0.75f)
	{
		// Fit to valid range by leveling off intensity
		float Tonemapped = (MaxValue - 0.75 * 0.75) / (MaxValue - 0.5);
		Color *= Tonemapped / MaxValue;
		MaxValue = Tonemapped;
	}

	Encoded.A = FMath::Min(FMath::CeilToInt(MaxValue * 255.0f), 255);
	Encoded.R = FMath::RoundToInt((Color.R * 255.0f / Encoded.A) * 255.0f);
	Encoded.G = FMath::RoundToInt((Color.G * 255.0f / Encoded.A) * 255.0f);
	Encoded.B = FMath::RoundToInt((Color.B * 255.0f / Encoded.A) * 255.0f);

	return Encoded;
}

static void EdgeWalkSetup(bool ReverseDirection, int32 Edge, int32 MipSize, int32& EdgeStart, int32& EdgeStep)
{
	if (ReverseDirection)
	{
		switch (Edge)
		{
		case EDGE_LEFT:		//start at lower left and walk up
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_RIGHT:	//start at lower right and walk up
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_TOP:		//start at upper right and walk left
			EdgeStart = (MipSize - 1);
			EdgeStep = -1;
			break;
		case EDGE_BOTTOM:	//start at lower right and walk left
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -1;
			break;
		}
	}
	else
	{
		switch (Edge)
		{
		case EDGE_LEFT:		//start at upper left and walk down
			EdgeStart = 0;
			EdgeStep = MipSize;
			break;
		case EDGE_RIGHT:	//start at upper right and walk down
			EdgeStart = (MipSize - 1);
			EdgeStep = MipSize;
			break;
		case EDGE_TOP:		//start at upper left and walk left
			EdgeStart = 0;
			EdgeStep = 1;
			break;
		case EDGE_BOTTOM:	//start at lower left and walk left
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = 1;
			break;
		}
	}
}

TRefCountPtr<FReflectionCaptureUncompressedData> GenerateFromDerivedDataSource(const FReflectionCaptureFullHDR& FullHDRData)
{
	const int32 NumMips = FMath::CeilLogTwo(FullHDRData.CubemapSize) + 1;

	TRefCountPtr<FReflectionCaptureUncompressedData> SourceCubemapData = FullHDRData.GetUncompressedData();

	int32 SourceMipBaseIndex = 0;
	int32 DestMipBaseIndex = 0;

	TRefCountPtr<FReflectionCaptureUncompressedData> CapturedData = new FReflectionCaptureUncompressedData(SourceCubemapData->Size() * sizeof(FColor) / sizeof(FFloat16Color));

	// Note: change REFLECTIONCAPTURE_ENCODED_DERIVEDDATA_VER when modifying the encoded data layout or contents

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const int32 SourceCubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);
		const int32 DestCubeFaceBytes = MipSize * MipSize * sizeof(FColor);

		const FFloat16Color*	MipSrcData = (const FFloat16Color*)SourceCubemapData->GetData(SourceMipBaseIndex);
		FColor*					MipDstData = (FColor*)CapturedData->GetData(DestMipBaseIndex);

		// Fix cubemap seams by averaging colors across edges

		int32 CornerTable[4] =
		{
			0,
			MipSize - 1,
			MipSize * (MipSize - 1),
			MipSize * (MipSize - 1) + MipSize - 1,
		};

		// Average corner colors
		FLinearColor AvgCornerColors[8];
		memset(AvgCornerColors, 0, sizeof(AvgCornerColors));
		for (int32 Face = 0; Face < CubeFace_MAX; Face++)
		{
			const FFloat16Color* FaceSrcData = MipSrcData + Face * MipSize * MipSize;

			for (int32 Corner = 0; Corner < 4; Corner++)
			{
				AvgCornerColors[CubeCornerList[Face][Corner]] += FLinearColor(FaceSrcData[CornerTable[Corner]]);
			}
		}

		// Encode corners
		for (int32 Face = 0; Face < CubeFace_MAX; Face++)
		{
			FColor* FaceDstData = MipDstData + Face * MipSize * MipSize;

			for (int32 Corner = 0; Corner < 4; Corner++)
			{
				const FLinearColor LinearColor = AvgCornerColors[CubeCornerList[Face][Corner]] / 3.0f;
				FaceDstData[CornerTable[Corner]] = RGBMEncode(LinearColor);
			}
		}

		// Average edge colors
		for (int32 EdgeIndex = 0; EdgeIndex < 12; EdgeIndex++)
		{
			int32 FaceA = CubeEdgeListA[EdgeIndex][0];
			int32 EdgeA = CubeEdgeListA[EdgeIndex][1];

			int32 FaceB = CubeEdgeListB[EdgeIndex][0];
			int32 EdgeB = CubeEdgeListB[EdgeIndex][1];

			const FFloat16Color*	FaceSrcDataA = MipSrcData + FaceA * MipSize * MipSize;
			FColor*					FaceDstDataA = MipDstData + FaceA * MipSize * MipSize;

			const FFloat16Color*	FaceSrcDataB = MipSrcData + FaceB * MipSize * MipSize;
			FColor*					FaceDstDataB = MipDstData + FaceB * MipSize * MipSize;

			int32 EdgeStartA = 0;
			int32 EdgeStepA = 0;
			int32 EdgeStartB = 0;
			int32 EdgeStepB = 0;

			EdgeWalkSetup(false, EdgeA, MipSize, EdgeStartA, EdgeStepA);
			EdgeWalkSetup(EdgeA == EdgeB || EdgeA + EdgeB == 3, EdgeB, MipSize, EdgeStartB, EdgeStepB);

			// Walk edge
			// Skip corners
			for (int32 Texel = 1; Texel < MipSize - 1; Texel++)
			{
				int32 EdgeTexelA = EdgeStartA + EdgeStepA * Texel;
				int32 EdgeTexelB = EdgeStartB + EdgeStepB * Texel;

				check(0 <= EdgeTexelA && EdgeTexelA < MipSize * MipSize);
				check(0 <= EdgeTexelB && EdgeTexelB < MipSize * MipSize);

				const FLinearColor EdgeColorA = FLinearColor(FaceSrcDataA[EdgeTexelA]);
				const FLinearColor EdgeColorB = FLinearColor(FaceSrcDataB[EdgeTexelB]);
				const FLinearColor AvgColor = 0.5f * (EdgeColorA + EdgeColorB);

				FaceDstDataA[EdgeTexelA] = FaceDstDataB[EdgeTexelB] = RGBMEncode(AvgColor);
			}
		}

		// Encode rest of texels
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			const int32 FaceSourceIndex = SourceMipBaseIndex + CubeFace * SourceCubeFaceBytes;
			const int32 FaceDestIndex = DestMipBaseIndex + CubeFace * DestCubeFaceBytes;
			const FFloat16Color* FaceSourceData = (const FFloat16Color*)SourceCubemapData->GetData(FaceSourceIndex);
			FColor* FaceDestData = (FColor*)CapturedData->GetData(FaceDestIndex);

			// Convert each texel from linear space FP16 to RGBM FColor
			// Note: Brightness on the capture is baked into the encoded HDR data
			// Skip edges
			for (int32 y = 1; y < MipSize - 1; y++)
			{
				for (int32 x = 1; x < MipSize - 1; x++)
				{
					int32 TexelIndex = x + y * MipSize;
					const FLinearColor LinearColor = FLinearColor(FaceSourceData[TexelIndex]);
					FaceDestData[TexelIndex] = RGBMEncode(LinearColor);
				}
			}
		}

		SourceMipBaseIndex += SourceCubeFaceBytes * CubeFace_MAX;
		DestMipBaseIndex += DestCubeFaceBytes * CubeFace_MAX;
	}
	return CapturedData;
}

void CubeFace2UNITY(int32 face, FMatrix &matrix, int32 size)
{
	FMatrix temp1, temp2, temp3;
	matrix.SetIdentity();
	temp1.SetIdentity();
	temp1.SetOrigin(FVector(-0.5f * (size - 1), -0.5f * (size - 1), 0.f));
	temp2.SetIdentity();
	temp3.SetIdentity();
	temp3.SetOrigin(FVector(0.5f * (size - 1), 0.5f * (size - 1), 0.f));
	switch (face)
	{
	case 0:
	{
		temp2.M[0][0] = 0;
		temp2.M[0][1] = -1;
		temp2.M[1][0] = 1;
		temp2.M[1][1] = 0;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 1:
	{
		temp2.M[0][0] = 0;
		temp2.M[0][1] = 1;
		temp2.M[1][0] = -1;
		temp2.M[1][1] = 0;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 3:
	{
		temp2.M[0][0] = 1;
		//temp2.M[0][1] = 1;
		//temp2.M[1][0] = -1;
		temp2.M[1][1] = -1;
		matrix = temp1*temp2*temp3;
	}
	break;
	case 5:
	{
		temp2.M[0][0] = -1;
		//temp2.M[0][1] = 1;
		//temp2.M[1][0] = -1;
		temp2.M[1][1] = -1;
		matrix = temp1*temp2*temp3;
	}
	break;
	}
}

void GetFaceData(TArray<uint8> &writeData, const uint8 *data, int32 face, int32 size)
{
	FMatrix matrix;
	CubeFace2UNITY(face, matrix, size);
	for (int32 y = 0; y < size; ++y)
	{
		for (int32 x = 0; x < size; ++x)
		{
			FVector v = matrix.TransformPosition(FVector(x, y, 0));
			writeData[v.Y*size * 4 + v.X * 4] = data[y*size * 4 + x * 4];
			writeData[v.Y*size * 4 + v.X * 4 + 1] = data[y*size * 4 + x * 4 + 1];
			writeData[v.Y*size * 4 + v.X * 4 + 2] = data[y*size * 4 + x * 4 + 2];
			writeData[v.Y*size * 4 + v.X * 4 + 3] = data[y*size * 4 + x * 4 + 3];
		}
	}
}

template <class T>
IFileHandle& operator << (IFileHandle& kFile, T tVal)
{
	kFile.Write((const uint8*)&tVal, sizeof(T));
	return kFile;
}

void Write(IFileHandle& kFile, const FString& strVal)
{
	for (int32 i(0); i < strVal.Len(); ++i)
	{
		kFile << (char)strVal[i];
	}
	kFile << '\0';
}

void Write(IFileHandle& kFile, FVector kPos)
{
	kFile << kPos.X;
	kFile << kPos.Z;
	kFile << kPos.Y;
}

void WritePosition(IFileHandle& kFile, FVector kPos)
{
	kFile << kPos.X * -0.01f;
	kFile << kPos.Z * 0.01f;
	kFile << kPos.Y * 0.01f;
}

void WriteRotation(IFileHandle& kFile, FQuat kRot)
{
	FVector kEuler = kRot.Euler();
	kFile << kEuler.X;
	kFile << kEuler.Y;
	kFile << kEuler.Z;
}

void WriteScale(IFileHandle& kFile, FVector kScale)
{
	kFile << kScale.X;
	kFile << kScale.Y;
	kFile << kScale.Z;
}

void Write(IFileHandle& kFile, FTransform& kTransform)
{
	WritePosition(kFile, kTransform.GetLocation());
	WriteRotation(kFile, kTransform.GetRotation());
	WriteScale(kFile, kTransform.GetScale3D());
}

UDirectionalLightComponent* GetDirectionalLightComponent(AActor* pkActor)
{
	TArray<UDirectionalLightComponent*> aryLightComponents;
	pkActor->GetComponents(aryLightComponents);
	if (aryLightComponents.Num())
	{
		return aryLightComponents[0];
	}
	else
	{
		return nullptr;
	}
}

UExponentialHeightFogComponent* GetExponentialHeightFogComponent(AActor* pkActor)
{
	TArray<UExponentialHeightFogComponent*> aryFogComponents;
	pkActor->GetComponents(aryFogComponents);
	if (aryFogComponents.Num())
	{
		return aryFogComponents[0];
	}
	else
	{
		return nullptr;
	}
}

class ShadowMap2DExt : public FShadowMap2D
{
public:
	FVector4 GetInvUniformPenumbraSize()
	{
		return InvUniformPenumbraSize;
	}
};

class FPrecomputedLightVolumeDataExt
{
public:
	bool bInitialized;
	FBox Bounds;

	/** Octree containing lighting samples to be used with high quality lightmaps. */
	FLightVolumeOctree HighQualityLightmapOctree;

	/** Octree containing lighting samples to be used with low quality lightmaps. */
	FLightVolumeOctree LowQualityLightmapOctree;
};

class ExportingProcess;
static TSharedPtr<ExportingProcess> s_spProcess;

class ExportingProcess : public FRunnable, FSingleThreadRunnable
{
public:
	enum EndState
	{
		ES_FAILED,
		ES_SUCCEEDED,
		ES_CANCELED
	};

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

	struct LayerInfo
	{
		int32 layerIdx;
		int32 texIdx;
		int32 channel;
	};

	struct LandscapeComponent
	{
		int32 m_i32SectionBaseX;
		int32 m_i32SectionBaseY;
		UTexture2D* m_pkHeightMapNormal = nullptr;
		FLightMap2D* m_pkLightMap = nullptr;
		FShadowMap2D* m_pkShadowMap = nullptr;
		TArray<LayerInfo> m_aryLayerInfo;
		TArray<UTexture2D*> m_aryLayerTextures;

	};

	struct LandscapeLayer
	{
		float m_fTiling = 0;
		float m_fYaw = 0;
		float m_fPitch = 0;
		float m_fSpecular = 0;
		UTexture* m_pkBase = nullptr;
		UTexture* m_pkHeight = nullptr;
		UTexture* m_pkNormal = nullptr;
		UTexture* m_pkRoughness = nullptr;
	};

	struct LandscapeInfo
	{
		int32 m_i32ComponentSizeQuads;
		float m_f32LightingResolution;
		FString m_strName;
		FVector m_v3Position;
		FVector m_v3Scale;
		TMap<FIntPoint, LandscapeComponent> m_mapXYtoComponentMap;
		TMap<int32, LandscapeLayer> m_mapUsedLayers;
	};

	struct MaterialInfo
	{
		FString m_strName;
		int	m_index;
		SupportedMaterialType m_eType = MAT_MAX;
		TArray<UTexture*> m_aryRelatedTextures;
		TArray<float> m_aryRelatedParams;
	};

	struct StaticMeshInfo
	{
		FString m_strName;
		FString m_strFBXName;
		FTransform m_kTransform;
		TArray<MaterialInfo> m_kMaterials;
		FLightMap2D* m_pkLightMap = nullptr;
		FShadowMap2D* m_pkShadowMap = nullptr;
	};

	struct ReflectionInfo
	{
		FString m_strName;
		FVector m_v3Position;
		FVector m_v3Offset;
		float m_fBrightness;
		float m_fInfluenceRadius;
		float m_fAverageBrightness;
		const FReflectionCaptureFullHDR* m_pkData = nullptr;
	};

	struct LightMapInfo
	{
		UTexture2D* m_pkSource = nullptr;
		TArray<uint8> m_aryData;
	};

	struct ShadowMapInfo
	{
		UShadowMapTexture2D* m_pkSource = nullptr;
		TArray<uint8> m_aryData;
	};

	ExportingProcess(const FString& kPath)
		: m_kPath(kPath)
	{

	}

	~ExportingProcess()
	{
		StopThread();
		StopMainTick();
		while (m_kFGTasks.size())
		{
			auto func = m_kFGTasks.pop();
			if (func) delete func;
		}

		while (m_kBGTasks.size())
		{
			auto func = m_kBGTasks.pop();
			if (func) delete func;
		}
	}

	void Cancel()
	{
		m_bCanceling = true;
	}

	FTimespan GetDuration() const
	{
		if (m_bIsRunning)
		{
			return (FDateTime::UtcNow() - m_tStartTime);
		}

		return (m_kEndTime - m_tStartTime);
	}

	bool IsRunning() const
	{
		return m_bIsRunning;
	}

	void Launch(const TSharedPtr<SNotificationItem>& spItem)
	{
		check((!m_bIsRunning) && m_pkThread == nullptr && (!m_wpNotificationItem.IsValid()));
		m_wpNotificationItem = spItem;

		if (!Assigning())
		{
			ShowResult(ES_FAILED);
			Destory();
		}

		m_bIsRunning = true;
		m_tStartTime = FDateTime::UtcNow();

		m_hMainTick = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &ExportingProcess::HandleTicker), 0);
		m_pkThread = FRunnableThread::Create(this, TEXT("SceneExporting"), 128 * 1024, TPri_AboveNormal);
	}

	void SetSleepInterval(float InSleepInterval)
	{
		m_fSleepInterval = InSleepInterval;
	}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		m_tStartTime = FDateTime::UtcNow();
		while (m_bIsRunning)
		{
			FPlatformProcess::Sleep(m_fSleepInterval);
			TickInternal();
		}
		return 0;
	}

	virtual void Stop() override
	{
		Cancel();
	}

	virtual void Exit() override
	{

	}

	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

	void Tick() override
	{
		if (m_bIsRunning)
		{
			TickInternal();
		}
	}

	void TickInternal()
	{
		while (!m_bCanceling)
		{
			auto func = m_kBGTasks.pop();
			if (func)
			{
				(*func)();
				delete func;
			}
			else if (m_bExiting)
			{
				break;
			}
			else
			{
				FPlatformProcess::Sleep(0.1f);
			}
		}
		m_bIsRunning = false;
	}

	bool HandleTicker(float fDeltaTime)
	{
		if (m_bCanceling)
		{
			if (!m_bIsRunning)
			{
				ShowResult(ES_CANCELED);
				Destory();
			}
		}
		else
		{
			auto func = m_kFGTasks.pop();
			if (func)
			{
				(*func)();
				delete func;
			}
			else if (m_bExiting)
			{
				if (!m_bIsRunning)
				{
					ShowResult(ES_SUCCEEDED);
					Destory();
				}
			}
		}
		return true;
	}

private:
	static SupportedMaterialType GetType(UMaterialInterface& kMat)
	{
		UMaterial* pkBase = kMat.GetBaseMaterial();
		if (pkBase)
		{
			FString strName = pkBase->GetName();
			if (strName == "SceneGrass")
			{
				return MAT_SCENE_GRASS;
			}
			else if (strName == "ScenePlain")
			{
				return MAT_SCENE_PLAIN;
			}
			else if (strName == "ScenePlainAlpha")
			{
				return MAT_SCENE_PLAIN_ALPHA;
			}
			else if (strName == "ScenePBR")
			{
				return MAT_SCENE_PBR;
			}
			else if (strName == "ScenePBRAlpha")
			{
				return MAT_SCENE_PBR_ALPHA;
			}
			else if (strName == "ScenePBRGlow")
			{
				return MAT_SCENE_PBR_GLOW;
			}
			else if (strName == "TerrainPBR")
			{
				return MAT_TERRAIN_PBR;
			}
			else if (strName == "Lake")
			{
				return MAT_WATER;
			}
		}
		return MAT_MAX;
	}

	int GetMaterialIndex(FStaticMeshLODResources& LOD, int nIndex)
	{
		for (int i = 0; i < LOD.Sections.Num(); ++i)
		{
			if (LOD.Sections[i].MaterialIndex == nIndex)
			{
				return i;
			}
		}
		return -1;
	}

	bool Assigning()
	{
		m_pkMeshExporter = GetFBXExporter();
		m_pkTGAExporter = GetTGAExporter();
		if ((!m_pkMeshExporter) || (!m_pkTGAExporter)) return false;
		UWorld* pkWorld = GEditor->GetEditorWorldContext().World();
		if (!pkWorld) return false;
		m_kWorldName = pkWorld->GetName();

		{
			FString kPath = m_kPath + "/" + m_kWorldName;
			IPlatformFile& kPlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
			kPath = m_kPath + "/" + m_kWorldName + "/Meshes";
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
			kPath = m_kPath + "/" + m_kWorldName + "/Textures";
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
			kPath = m_kPath + "/" + m_kWorldName + "/LightMaps";
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
			kPath = m_kPath + "/" + m_kWorldName + "/EnvMaps";
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
			kPath = m_kPath + "/" + m_kWorldName + "/Materials";
			if (!kPlatformFile.DirectoryExists(*kPath))
			{
				kPlatformFile.CreateDirectory(*kPath);
			}
		}

		m_kLandscape.m_i32ComponentSizeQuads = 0;
		m_kLandscape.m_strName = "";
		m_kLandscape.m_mapXYtoComponentMap.Reset();
		
		for (auto pkLevel : pkWorld->GetLevels())
		{
			m_kFGTasks.push(new std::function<void()>(
				[this, pkLevel]()
			{
				for (auto pkActor : pkLevel->Actors)
				{
					if (pkActor->IsA(ASphereReflectionCapture::StaticClass()))
					{
						TArray<UReflectionCaptureComponent*> aryCaptures;
						pkActor->GetComponents(aryCaptures);
						if (aryCaptures.Num() > 0 && aryCaptures[0])
						{
							const FReflectionCaptureFullHDR* pkData = aryCaptures[0]->GetFullHDRData();
							if (pkData)
							{
								ReflectionInfo& kInfo = m_aryReflectionProbes[m_aryReflectionProbes.AddDefaulted(1)];
								kInfo.m_strName = pkActor->GetName();
								{
									int& iNameCount = m_mapInvolvedActorNames.FindOrAdd(kInfo.m_strName);
									if (iNameCount > 0)
									{
										kInfo.m_strName = kInfo.m_strName + FString::Printf(TEXT("_%d"), iNameCount);
									}
									++iNameCount;
								}
								kInfo.m_v3Position = aryCaptures[0]->ComponentToWorld.ToMatrixWithScale().GetOrigin();
								kInfo.m_v3Offset = aryCaptures[0]->CaptureOffset;
								kInfo.m_fInfluenceRadius = aryCaptures[0]->GetInfluenceBoundingRadius();
								kInfo.m_fBrightness = aryCaptures[0]->Brightness;
								kInfo.m_fAverageBrightness = aryCaptures[0]->GetAverageBrightness();
								kInfo.m_pkData = pkData;
							}
						}
					}
					else if (pkActor->IsA(ADirectionalLight::StaticClass()))
					{
						UDirectionalLightComponent* pkLightCompont = GetDirectionalLightComponent(pkActor);
						if ((!m_pkMainLight) && pkLightCompont && pkLightCompont->Mobility == EComponentMobility::Stationary)
						{
							m_pkMainLight = pkLightCompont;
						}
					}
					else if (pkActor->IsA(AExponentialHeightFog::StaticClass()))
					{
						UExponentialHeightFogComponent* pkHeightFog = GetExponentialHeightFogComponent(pkActor);
						if ((!m_pkMainFog) && pkHeightFog)
						{
							m_pkMainFog = pkHeightFog;
						}
					}
					else if (pkActor->IsA(ALandscape::StaticClass()))
					{
						ULandscapeInfo* pkInfo = CastChecked<ALandscape>(pkActor)->GetLandscapeInfo();
						UMaterialInterface* pkMaterial = CastChecked<ALandscape>(pkActor)->GetLandscapeMaterial();
						if (!pkMaterial) continue;
						UMaterial* pkBase = pkMaterial->GetBaseMaterial();
						if ((!pkBase) || pkBase->GetName() != "LandscapeBase") continue;
						
						if (m_kLandscape.m_i32ComponentSizeQuads == 0
							&& pkInfo->ComponentNumSubsections == 1
							&& pkInfo->ComponentSizeQuads == pkInfo->SubsectionSizeQuads)
						{
							m_kLandscape.m_i32ComponentSizeQuads = pkInfo->ComponentSizeQuads;
							m_kLandscape.m_f32LightingResolution = pkInfo->LandscapeActor->StaticLightingResolution;
							m_kLandscape.m_strName = pkActor->GetName();
							m_kLandscape.m_v3Position = pkActor->GetTransform().GetLocation();
							m_kLandscape.m_v3Scale = pkInfo->DrawScale * 0.01f;
							m_kLandscape.m_mapUsedLayers.Reset();
							for (auto& itComponent : pkInfo->XYtoComponentMap)
							{
								auto& kInfo = m_kLandscape.m_mapXYtoComponentMap.Add(itComponent.Get<0>());
								ULandscapeComponent* pkComponent = itComponent.Get<1>();
								kInfo.m_i32SectionBaseX = pkComponent->SectionBaseX;
								kInfo.m_i32SectionBaseY = pkComponent->SectionBaseY;
								kInfo.m_pkHeightMapNormal = pkComponent->HeightmapTexture;
								const FMeshMapBuildData* pkMeshMapBuildData = pkComponent->GetMeshMapBuildData();
								if (pkMeshMapBuildData)
								{
									if (pkMeshMapBuildData->LightMap != nullptr)
									{
										kInfo.m_pkLightMap = pkMeshMapBuildData->LightMap->GetLightMap2D();
										UTexture2D* pkLightMapTex = kInfo.m_pkLightMap->GetTexture(1);
										m_mapLightMaps.FindOrAdd(pkLightMapTex->GetName()).m_pkSource = pkLightMapTex;
										if (pkMeshMapBuildData->ShadowMap != nullptr)
										{
											kInfo.m_pkShadowMap = pkMeshMapBuildData->ShadowMap->GetShadowMap2D();
											UShadowMapTexture2D* pkShadowMapTex = kInfo.m_pkShadowMap->GetTexture();
											m_mapShadowMaps.FindOrAdd(pkShadowMapTex->GetName()).m_pkSource = pkShadowMapTex;
										}
									}
								}
								
								for (int32 i(0); i < pkComponent->WeightmapLayerAllocations.Num(); ++i)
								{
									FWeightmapLayerAllocationInfo& kLayerInfo = pkComponent->WeightmapLayerAllocations[i];
									if (kLayerInfo.LayerInfo)
									{
										FString kName = kLayerInfo.LayerInfo->LayerName.ToString();
										int32 idx = kName.Find("Layer_");
										if (idx == 0)
										{
											LayerInfo info;
											info.layerIdx = char_num(kName[6]);
											m_kLandscape.m_mapUsedLayers.FindOrAdd(info.layerIdx);
											info.texIdx = kLayerInfo.WeightmapTextureIndex;
											info.channel = kLayerInfo.WeightmapTextureChannel;
											if (info.channel == 0) info.channel = 2;
											else if (info.channel == 2) info.channel = 0;
											kInfo.m_aryLayerInfo.Push(info);
										}
									}
								}
								kInfo.m_aryLayerTextures = pkComponent->WeightmapTextures;
							}
							
							for (auto& layer : m_kLandscape.m_mapUsedLayers)
							{
								TCHAR suffix[2];
								suffix[0] = num_char(layer.Key);
								suffix[1] = 0;
								TCHAR buffer[256];
								_sntprintf_s(buffer, 256, TEXT("0.Tiling_%s"), suffix);
								pkMaterial->GetScalarParameterValue(buffer, layer.Value.m_fTiling);
								_sntprintf_s(buffer, 256, TEXT("1.Yaw_%s"), suffix);
								pkMaterial->GetScalarParameterValue(buffer, layer.Value.m_fYaw);
								_sntprintf_s(buffer, 256, TEXT("2.Pitch_%s"), suffix);
								pkMaterial->GetScalarParameterValue(buffer, layer.Value.m_fPitch);
								_sntprintf_s(buffer, 256, TEXT("3.Specular_%s"), suffix);
								pkMaterial->GetScalarParameterValue(buffer, layer.Value.m_fSpecular);
								_sntprintf_s(buffer, 256, TEXT("BaseMap_%s"), suffix);
								pkMaterial->GetTextureParameterValue(buffer, layer.Value.m_pkBase);
								_sntprintf_s(buffer, 256, TEXT("HeightMap_%s"), suffix);
								pkMaterial->GetTextureParameterValue(buffer, layer.Value.m_pkHeight);
								_sntprintf_s(buffer, 256, TEXT("NormalMap_%s"), suffix);
								pkMaterial->GetTextureParameterValue(buffer, layer.Value.m_pkNormal);
								_sntprintf_s(buffer, 256, TEXT("Roughness_%s"), suffix);
								pkMaterial->GetTextureParameterValue(buffer, layer.Value.m_pkRoughness);
							}
						}
					}
					else if (pkActor->IsA(AStaticMeshActor::StaticClass()))
					{
						UStaticMeshComponent* pkStaticMesh = CastChecked<AStaticMeshActor>(pkActor)->GetStaticMeshComponent();
						if (!pkStaticMesh) continue;
						UMaterialInterface* pkMaterial = pkStaticMesh->GetMaterial(0);
						if (!pkMaterial) continue;
						SupportedMaterialType eMatType = GetType(*pkMaterial);
						if (eMatType >= MAT_MAX) continue;
						StaticMeshInfo& kInfo = m_aryStaticMeshes[m_aryStaticMeshes.AddDefaulted(1)];
						kInfo.m_strName = pkActor->GetName();
						{
							int& iNameCount = m_mapInvolvedActorNames.FindOrAdd(kInfo.m_strName);
							if (iNameCount > 0)
							{
								kInfo.m_strName = kInfo.m_strName + FString::Printf(TEXT("_%d"), iNameCount);
							}
							++iNameCount;
						}
						kInfo.m_strFBXName = pkStaticMesh->GetStaticMesh()->GetName();
						m_mapFBXMeshes.FindOrAdd(kInfo.m_strFBXName) = pkStaticMesh->GetStaticMesh();
						kInfo.m_kTransform = pkActor->GetTransform();

						if ("S_Rongyan_MB_006_02_75" == kInfo.m_strName)
						{
							int a = 0;
						}

						FStaticMeshLODResources& LOD = pkStaticMesh->GetStaticMesh()->RenderData->LODResources[0];
						int matIndex = 0;
						while(true)
						{
							MaterialInfo& matInfo = kInfo.m_kMaterials[kInfo.m_kMaterials.AddDefaulted(1)];
							matInfo.m_strName = pkMaterial->GetName();
							matInfo.m_index = GetMaterialIndex(LOD, matIndex);

							if (matInfo.m_index == -1)
							{
								int kkkk = 0;
							}


							matInfo.m_eType = eMatType;
							switch (matInfo.m_eType)
							{
							case MAT_SCENE_GRASS:
								matInfo.m_aryRelatedTextures.SetNum(1);
								pkMaterial->GetTextureParameterValue(TEXT("BaseTexture"), matInfo.m_aryRelatedTextures[0]);
								matInfo.m_aryRelatedParams.SetNum(1);
								pkMaterial->GetScalarParameterValue(TEXT("LightIntensity"), matInfo.m_aryRelatedParams[0]);
								break;
							case MAT_SCENE_PLAIN:
							case MAT_SCENE_PLAIN_ALPHA:
								matInfo.m_aryRelatedTextures.SetNum(1);
								pkMaterial->GetTextureParameterValue(TEXT("BaseTexture"), matInfo.m_aryRelatedTextures[0]);
								break;
							case MAT_SCENE_PBR:
							case MAT_SCENE_PBR_ALPHA:
								matInfo.m_aryRelatedTextures.SetNum(3);
								pkMaterial->GetTextureParameterValue(TEXT("BaseTexture"), matInfo.m_aryRelatedTextures[0]);
								pkMaterial->GetTextureParameterValue(TEXT("MixTexture"), matInfo.m_aryRelatedTextures[1]);
								pkMaterial->GetTextureParameterValue(TEXT("NormalTexture"), matInfo.m_aryRelatedTextures[2]);
								break;
							case MAT_SCENE_PBR_GLOW:
								matInfo.m_aryRelatedTextures.SetNum(4);
								pkMaterial->GetTextureParameterValue(TEXT("BaseTexture"), matInfo.m_aryRelatedTextures[0]);
								pkMaterial->GetTextureParameterValue(TEXT("MixTexture"), matInfo.m_aryRelatedTextures[1]);
								pkMaterial->GetTextureParameterValue(TEXT("NormalTexture"), matInfo.m_aryRelatedTextures[2]);
								pkMaterial->GetTextureParameterValue(TEXT("GlowTexture"), matInfo.m_aryRelatedTextures[3]);
								matInfo.m_aryRelatedParams.SetNum(1);
								pkMaterial->GetScalarParameterValue(TEXT("GlowIntensity"), matInfo.m_aryRelatedParams[0]);
								break;
							case MAT_TERRAIN_PBR:
								matInfo.m_aryRelatedTextures.SetNum(6);
								pkMaterial->GetTextureParameterValue(TEXT("BasePBR"), matInfo.m_aryRelatedTextures[0]);
								pkMaterial->GetTextureParameterValue(TEXT("MixPBR"), matInfo.m_aryRelatedTextures[1]);
								pkMaterial->GetTextureParameterValue(TEXT("NormalPBR"), matInfo.m_aryRelatedTextures[2]);
								pkMaterial->GetTextureParameterValue(TEXT("BaseLayer0"), matInfo.m_aryRelatedTextures[3]);
								pkMaterial->GetTextureParameterValue(TEXT("BaseLayer1"), matInfo.m_aryRelatedTextures[4]);
								pkMaterial->GetTextureParameterValue(TEXT("Blend"), matInfo.m_aryRelatedTextures[5]);
								matInfo.m_aryRelatedParams.SetNum(3);
								pkMaterial->GetScalarParameterValue(TEXT("TilingPBR"), matInfo.m_aryRelatedParams[0]);
								pkMaterial->GetScalarParameterValue(TEXT("Tiling0"), matInfo.m_aryRelatedParams[1]);
								pkMaterial->GetScalarParameterValue(TEXT("Tiling1"), matInfo.m_aryRelatedParams[2]);
								break;
							case MAT_WATER:
								matInfo.m_aryRelatedTextures.SetNum(0);
								break;
							default:
								break;
							}
							pkMaterial = pkStaticMesh->GetMaterial(++matIndex);
							if (!pkMaterial) break;
							eMatType = GetType(*pkMaterial);
							if (eMatType >= MAT_MAX) break;
						}						

						if (pkStaticMesh->LODData.Num() > 0)
						{
							const FMeshMapBuildData* pkMeshMapBuildData = pkStaticMesh->GetMeshMapBuildData(pkStaticMesh->LODData[0]);
							if (pkMeshMapBuildData)
							{
								if (pkMeshMapBuildData->LightMap != nullptr)
								{
									kInfo.m_pkLightMap = pkMeshMapBuildData->LightMap->GetLightMap2D();
									UTexture2D* pkLightMapTex = kInfo.m_pkLightMap->GetTexture(1);
									m_mapLightMaps.FindOrAdd(pkLightMapTex->GetName()).m_pkSource = pkLightMapTex;

									if (pkMeshMapBuildData->ShadowMap != nullptr)
									{
										kInfo.m_pkShadowMap = pkMeshMapBuildData->ShadowMap->GetShadowMap2D();
										UShadowMapTexture2D* pkShadowMapTex = kInfo.m_pkShadowMap->GetTexture();
										m_mapShadowMaps.FindOrAdd(pkShadowMapTex->GetName()).m_pkSource = pkShadowMapTex;
									}
								}
							}
						}
					}
				}

				ExportMeshes();
				ExportTextures();
				ExportSceneStructure();

				m_kBGTasks.push(new std::function<void()>([this]()
				{
					ExportLightMaps();
					ExportReflectionProbes();
					m_bExiting = true;
				}));
			}));
		}

		for (auto vol : pkWorld->Scene->GetRenderScene()->PrecomputedLightVolumes)
		{
			FPrecomputedLightVolumeDataExt* pkData = (FPrecomputedLightVolumeDataExt*)(void*)(vol->Data);
			m_aryLightCacheData.Add(&(pkData->LowQualityLightmapOctree));
		}
		return true;
	}

	void ExportLightMaps()
	{
		for (auto& itTex : m_mapLightMaps)
		{
			LightMapInfo& kInfo = itTex.Get<1>();
			kInfo.m_pkSource->Source.GetMipData(kInfo.m_aryData, 0);
			int32 i(3);
			while (i < kInfo.m_aryData.Num())
			{
				kInfo.m_aryData[i] = 0;
				i += 4;
			}
		}

		for (auto& itTex : m_mapShadowMaps)
		{
			ShadowMapInfo& kInfo = itTex.Get<1>();
			kInfo.m_pkSource->Source.GetMipData(kInfo.m_aryData, 0);
		}

		for (auto& itMesh : m_aryStaticMeshes)
		{
			if (!itMesh.m_pkLightMap) continue;
			LightMapInfo* pkLMInfo = m_mapLightMaps.Find(itMesh.m_pkLightMap->GetTexture(1)->GetName());
			if (!pkLMInfo) continue;
			if (!itMesh.m_pkShadowMap) continue;
			ShadowMapInfo* pkSMInfo = m_mapShadowMaps.Find(itMesh.m_pkShadowMap->GetTexture()->GetName());
			if (!pkSMInfo) continue;

			FVector2D v2SrcScale = itMesh.m_pkShadowMap->GetCoordinateScale();
			FVector2D v2SrcBias = itMesh.m_pkShadowMap->GetCoordinateBias();
			FVector2D v2DstScale = itMesh.m_pkLightMap->GetCoordinateScale();
			FVector2D v2DstBias = itMesh.m_pkLightMap->GetCoordinateBias();

			uint32 stw = itMesh.m_pkShadowMap->GetTexture()->GetSizeX();
			uint32 sth = itMesh.m_pkShadowMap->GetTexture()->GetSizeY();
			uint32 dtw = itMesh.m_pkLightMap->GetTexture(1)->GetSizeX();
			uint32 dth = itMesh.m_pkLightMap->GetTexture(1)->GetSizeY();

			uint32 sw = po2((uint32)(float(stw) * v2SrcScale.X));
			uint32 sh = po2((uint32)(float(sth) * v2SrcScale.Y));
			uint32 dw = po2((uint32)(float(dtw) * v2DstScale.X));
			uint32 dh = po2((uint32)(float(dth >> 1) * v2DstScale.Y));
			if (sw != dw || sh != dh) continue;

			uint32 sx = roundpos(float(stw) * v2SrcBias.X, sw);
			uint32 sy = roundpos(float(sth) * v2SrcBias.Y, sh);
			uint32 dx = roundpos(float(dtw) * v2DstBias.X, dw);
			uint32 dy = roundpos(float(dth >> 1) * v2DstBias.Y, dh);

			for (uint32 i(0); i < sw; ++i)
			{
				for (uint32 j(0); j < sh; ++j)
				{
					pkLMInfo->m_aryData[((j + dy) * dtw + (i + dx)) * 4 + 3] = pkSMInfo->m_aryData[(j + sy) * stw + (i + sx)];
				}
			}
		}

		if (m_kLandscape.m_i32ComponentSizeQuads > 0)
		{
			for (auto& itComp : m_kLandscape.m_mapXYtoComponentMap)
			{
				LandscapeComponent& comp = itComp.Get<1>();

				if (!comp.m_pkLightMap) continue;
				LightMapInfo* pkLMInfo = m_mapLightMaps.Find(comp.m_pkLightMap->GetTexture(1)->GetName());
				if (!pkLMInfo) continue;
				if (!comp.m_pkShadowMap) continue;
				ShadowMapInfo* pkSMInfo = m_mapShadowMaps.Find(comp.m_pkShadowMap->GetTexture()->GetName());
				if (!pkSMInfo) continue;

				FVector2D v2SrcScale = comp.m_pkShadowMap->GetCoordinateScale();
				FVector2D v2SrcBias = comp.m_pkShadowMap->GetCoordinateBias();
				FVector2D v2DstScale = comp.m_pkLightMap->GetCoordinateScale();
				FVector2D v2DstBias = comp.m_pkLightMap->GetCoordinateBias();

				uint32 stw = comp.m_pkShadowMap->GetTexture()->GetSizeX();
				uint32 sth = comp.m_pkShadowMap->GetTexture()->GetSizeY();
				uint32 dtw = comp.m_pkLightMap->GetTexture(1)->GetSizeX();
				uint32 dth = comp.m_pkLightMap->GetTexture(1)->GetSizeY();

				uint32 sw = po2((uint32)(float(stw) * v2SrcScale.X));
				uint32 sh = po2((uint32)(float(sth) * v2SrcScale.Y));
				uint32 dw = po2((uint32)(float(dtw) * v2DstScale.X));
				uint32 dh = po2((uint32)(float(dth >> 1) * v2DstScale.Y));
				if (sw != dw || sh != dh) continue;

				uint32 sx = roundpos(float(stw) * v2SrcBias.X, sw);
				uint32 sy = roundpos(float(sth) * v2SrcBias.Y, sh);
				uint32 dx = roundpos(float(dtw) * v2DstBias.X, dw);
				uint32 dy = roundpos(float(dth >> 1) * v2DstBias.Y, dh);

				for (uint32 i(0); i < sw; ++i)
				{
					for (uint32 j(0); j < sh; ++j)
					{
						pkLMInfo->m_aryData[((j + dy) * dtw + (i + dx)) * 4 + 3] = pkSMInfo->m_aryData[(j + sy) * stw + (i + sx)];
					}
				}
			}
		}

		for (auto& itTex : m_mapLightMaps)
		{
			LightMapInfo& kInfo = itTex.Get<1>();
			FString kFileName = m_kPath + "/" + m_kWorldName + "/LightMaps/" + itTex.Get<0>() + ".tga";
			IFileHandle* hFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*kFileName);
			if (hFile)
			{
				uint8 acHeader[12] = { 0,0,2,0,0,0,0,0,0,0,0,0 };
				hFile->Write(acHeader, 12);
				((uint16*)acHeader)[0] = kInfo.m_pkSource->GetSizeX();
				((uint16*)acHeader)[1] = kInfo.m_pkSource->GetSizeY();
				acHeader[4] = 32;
				acHeader[5] = 8;
				hFile->Write(acHeader, 6);
				uint8* pbyBuffer = kInfo.m_aryData.GetData();
				uint32 u32Pitch = kInfo.m_pkSource->GetSizeX() * 4;
				for (int32 i(0); i < kInfo.m_pkSource->GetSizeY(); ++i)
				{
					hFile->Write(pbyBuffer + (kInfo.m_pkSource->GetSizeY() - i - 1) * u32Pitch, u32Pitch);
				}
				delete hFile;
				UE_LOG(SceneExporter, Log, TEXT("LightMap \"%s\" exported."), *kFileName);
				
			}
		}
	}

	void ExportMeshes()
	{
		for (auto& itMesh : m_mapFBXMeshes)
		{
			UExporter::FExportToFileParams kParams;
			kParams.Object = itMesh.Get<1>();
			kParams.Exporter = m_pkMeshExporter;
			FString kExportPath = m_kPath + "/" + m_kWorldName + "/Meshes/" + itMesh.Get<0>() + ".fbx";
			kParams.Filename = *kExportPath;
			kParams.InSelectedOnly = false;
			kParams.NoReplaceIdentical = false;
			kParams.Prompt = false;
			kParams.bUseFileArchive = false;
			kParams.WriteEmptyFiles = false;
			UExporter::ExportToFileEx(kParams);
			UE_LOG(SceneExporter, Log, TEXT("Mesh \"%s\" exported."), *kExportPath);
		}
	}

	void ExportTextures()
	{
		TSet<FString> kTexNames;
		for (auto& itMesh : m_aryStaticMeshes)
		{
			for (auto& itMaterial : itMesh.m_kMaterials)
			{
				for (auto& itTex : itMaterial.m_aryRelatedTextures)
				{
					if (!kTexNames.Find(itTex->GetName()))
					{
						kTexNames.Add(itTex->GetName());
						UExporter::FExportToFileParams kParams;
						kParams.Object = itTex;
						kParams.Exporter = m_pkTGAExporter;
						FString kExportPath = m_kPath + "/" + m_kWorldName + "/Textures/" + itTex->GetName() + ".tga";
						kParams.Filename = *kExportPath;
						kParams.InSelectedOnly = false;
						kParams.NoReplaceIdentical = false;
						kParams.Prompt = false;
						kParams.bUseFileArchive = false;
						kParams.WriteEmptyFiles = false;
						UExporter::ExportToFileEx(kParams);
						UE_LOG(SceneExporter, Log, TEXT("Texture \"%s\" exported."), *kExportPath);
					}
				}
			}
		}

		TArray<uint8> m_aryBase;
		TArray<uint8> m_aryHeight;
		TArray<uint8> m_aryNormal;
		TArray<uint8> m_aryRoughness;
		for (auto& layer : m_kLandscape.m_mapUsedLayers)
		{
			if (layer.Value.m_pkBase && layer.Value.m_pkRoughness
				&& layer.Value.m_pkBase->Source.GetSizeX() == layer.Value.m_pkRoughness->Source.GetSizeX()
				&& layer.Value.m_pkBase->Source.GetSizeY() == layer.Value.m_pkRoughness->Source.GetSizeY()
				&& layer.Value.m_pkNormal && layer.Value.m_pkHeight
				&& layer.Value.m_pkNormal->Source.GetSizeX() == layer.Value.m_pkHeight->Source.GetSizeX()
				&& layer.Value.m_pkNormal->Source.GetSizeY() == layer.Value.m_pkHeight->Source.GetSizeY()
				&& layer.Value.m_pkHeight->Source.GetBytesPerPixel() == 1
				&& layer.Value.m_pkRoughness->Source.GetBytesPerPixel() == 1)
			{
				layer.Value.m_pkBase->Source.GetMipData(m_aryBase, 0);
				layer.Value.m_pkHeight->Source.GetMipData(m_aryHeight, 0);
				layer.Value.m_pkNormal->Source.GetMipData(m_aryNormal, 0);
				layer.Value.m_pkRoughness->Source.GetMipData(m_aryRoughness, 0);
				for (int i(0); i < (int)(m_aryBase.Num() >> 2); ++i)
				{
					m_aryBase[i * 4 + 3] = m_aryRoughness[i];
				}
				{
					FString kFileName = m_kPath + "/" + m_kWorldName + "/Textures/" + layer.Value.m_pkBase->GetName() + ".tga";
					IFileHandle* hFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*kFileName);
					if (hFile)
					{
						uint8 acHeader[12] = { 0,0,2,0,0,0,0,0,0,0,0,0 };
						hFile->Write(acHeader, 12);
						((uint16*)acHeader)[0] = layer.Value.m_pkBase->Source.GetSizeX();
						((uint16*)acHeader)[1] = layer.Value.m_pkBase->Source.GetSizeY();
						acHeader[4] = 32;
						acHeader[5] = 8;
						hFile->Write(acHeader, 6);
						uint8* pbyBuffer = m_aryBase.GetData();
						uint32 u32Pitch = layer.Value.m_pkBase->Source.GetSizeX() * 4;
						for (int32 i(0); i < layer.Value.m_pkBase->Source.GetSizeY(); ++i)
						{
							hFile->Write(pbyBuffer + (layer.Value.m_pkBase->Source.GetSizeY() - i - 1) * u32Pitch, u32Pitch);
						}
						delete hFile;
						UE_LOG(SceneExporter, Log, TEXT("LayerMap \"%s\" exported."), *kFileName);
					}
				}
				for (int i(0); i < (int)(m_aryNormal.Num() >> 2); ++i)
				{
					m_aryNormal[i * 4 + 3] = m_aryHeight[i];
				}
				{
					FString kFileName = m_kPath + "/" + m_kWorldName + "/Textures/" + layer.Value.m_pkNormal->GetName() + ".tga";
					IFileHandle* hFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*kFileName);
					if (hFile)
					{
						uint8 acHeader[12] = { 0,0,2,0,0,0,0,0,0,0,0,0 };
						hFile->Write(acHeader, 12);
						((uint16*)acHeader)[0] = layer.Value.m_pkNormal->Source.GetSizeX();
						((uint16*)acHeader)[1] = layer.Value.m_pkNormal->Source.GetSizeY();
						acHeader[4] = 32;
						acHeader[5] = 8;
						hFile->Write(acHeader, 6);
						uint8* pbyBuffer = m_aryNormal.GetData();
						uint32 u32Pitch = layer.Value.m_pkNormal->Source.GetSizeX() * 4;
						for (int32 i(0); i < layer.Value.m_pkNormal->Source.GetSizeY(); ++i)
						{
							hFile->Write(pbyBuffer + (layer.Value.m_pkNormal->Source.GetSizeY() - i - 1) * u32Pitch, u32Pitch);
						}
						delete hFile;
						UE_LOG(SceneExporter, Log, TEXT("LayerMap \"%s\" exported."), *kFileName);
					}
				}
			}
		}
	}

	void ExportReflectionProbes()
	{
		TArray<uint8> writeData;
		for (auto& itProbe : m_aryReflectionProbes)
		{
			TRefCountPtr<FReflectionCaptureUncompressedData> rpCubemapData = GenerateFromDerivedDataSource(*itProbe.m_pkData);
			TArray<uint8>& aryData = rpCubemapData->GetArray();
			int32 CubemapSize = itProbe.m_pkData->CubemapSize;
			if (aryData.Num())
			{
				writeData.Empty(aryData.Num());
				writeData.AddZeroed(aryData.Num());
				CTexture texarray[6];
				int32 MipMapCount = FMath::Log2(CubemapSize) + 1;
				int32 MipBaseIndex = 0;
				int32 MipSize = 1 << (MipMapCount - 1);
				int32 CubeFaceBytes = MipSize * MipSize * 4;
				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					writeData.AddZeroed(CubeFaceBytes);
					const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
					GetFaceData(writeData, aryData.GetData() + SourceIndex, CubeFace, MipSize);
					texarray[CubeFace].create(CubemapSize, CubemapSize, 1, CubeFaceBytes, writeData.GetData()/* EncodedData.GetData()+SourceIndex*/);
				}
				MipBaseIndex += CubeFaceBytes * CubeFace_MAX;

				for (int32 MipIndex = 1; MipIndex < MipMapCount; MipIndex++)
				{
					MipSize = 1 << (MipMapCount - MipIndex - 1);
					CubeFaceBytes = MipSize * MipSize * 4;
					for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
					{
						writeData.AddZeroed(CubeFaceBytes);
						const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
						GetFaceData(writeData, aryData.GetData() + SourceIndex, CubeFace, MipSize);
						CSurface surface(MipSize, MipSize, 1, CubeFaceBytes, writeData.GetData()/*EncodedData.GetData() + SourceIndex*/);
						texarray[CubeFace].add_mipmap(surface);
					}
					MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
				}
				CDDSImage image;
				texarray[3].FlipX();
				image.create_textureCubemap(GL_BGRA_EXT, 4, texarray[0], texarray[1], texarray[5], texarray[4], texarray[2], texarray[3]);
				FString kExportPath = m_kPath + "/" + m_kWorldName + "/EnvMaps/" + itProbe.m_strName + ".dds";
				image.save(kExportPath);
				UE_LOG(SceneExporter, Log, TEXT("EnvMap \"%s\" exported."), *kExportPath);
			}
		}
	}

	void ExportSceneStructure()
	{
		FString kFileName = m_kPath + "/" + m_kWorldName + "/" + m_kWorldName + ".level";
		IFileHandle* hFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*kFileName);
		if (hFile)
		{
			if (m_pkMainLight)
			{
				(*hFile) << (uint32)(1);
				FVector DirectionalLightDirection = -m_pkMainLight->GetDirection();
				(*hFile) << -DirectionalLightDirection.X;
				(*hFile) << DirectionalLightDirection.Z;
				(*hFile) << DirectionalLightDirection.Y;

				FLinearColor DirectionalLightColor = FLinearColor(m_pkMainLight->LightColor) * m_pkMainLight->ComputeLightBrightness();
				if (m_pkMainLight->bUseTemperature)
				{
					DirectionalLightColor *= FLinearColor::MakeFromColorTemperature(m_pkMainLight->Temperature);
				}
				DirectionalLightColor /= PI;
				(*hFile) << DirectionalLightColor.R;
				(*hFile) << DirectionalLightColor.G;
				(*hFile) << DirectionalLightColor.B;
			}
			else
			{
				(*hFile) << (uint32)(0);
			}

			if (m_pkMainFog)
			{
				(*hFile) << (uint32)(1);
				(*hFile) << m_pkMainFog->GetComponentLocation().Z;
				(*hFile) << m_pkMainFog->FogDensity;
				(*hFile) << m_pkMainFog->FogInscatteringColor.R;
				(*hFile) << m_pkMainFog->FogInscatteringColor.G;
				(*hFile) << m_pkMainFog->FogInscatteringColor.B;
				(*hFile) << m_pkMainFog->FogInscatteringColor.A;
				(*hFile) << m_pkMainFog->FogHeightFalloff;
				(*hFile) << m_pkMainFog->FogMaxOpacity;
				(*hFile) << m_pkMainFog->StartDistance;
				(*hFile) << m_pkMainFog->FogCutoffDistance;
			}
			else
			{
				(*hFile) << (uint32)(0);
			}

			(*hFile) << (uint32)m_aryReflectionProbes.Num();
			for (auto& itRef : m_aryReflectionProbes)
			{
				Write(*hFile, itRef.m_strName);
				WritePosition(*hFile, itRef.m_v3Position);
				WritePosition(*hFile, itRef.m_v3Offset);
				(*hFile) << itRef.m_fBrightness;
				(*hFile) << itRef.m_fInfluenceRadius * 0.01f;
				(*hFile) << itRef.m_fAverageBrightness;
			}

			(*hFile) << (uint32)m_aryStaticMeshes.Num();
			for (auto& itMesh : m_aryStaticMeshes)
			{
				Write(*hFile, itMesh.m_strName);
				Write(*hFile, itMesh.m_strFBXName);
				Write(*hFile, itMesh.m_kTransform);
				(*hFile) << (uint32)itMesh.m_kMaterials.Num();

				for (auto& itMat : itMesh.m_kMaterials)
				{
					Write(*hFile, itMat.m_strName);
					(*hFile) << (uint32)itMat.m_index;
					(*hFile) << (uint32)itMat.m_eType;
					(*hFile) << (uint32)itMat.m_aryRelatedTextures.Num();
					for (auto& itTex : itMat.m_aryRelatedTextures)
					{
						Write(*hFile, itTex->GetName());
					}
					(*hFile) << (uint32)itMat.m_aryRelatedParams.Num();
					for (float fParam : itMat.m_aryRelatedParams)
					{
						(*hFile) << fParam;
					}
				}

				if ("S_ZhuCheng_MB_008_13" == itMesh.m_strFBXName)
				{
					int a = 0;
				}

				if (itMesh.m_pkLightMap)
				{
					
					Write(*hFile, ((LightMap2DExt*)itMesh.m_pkLightMap)->GetTexture(1)->GetName());
					(*hFile) << itMesh.m_pkLightMap->GetCoordinateScale().X;
					(*hFile) << itMesh.m_pkLightMap->GetCoordinateScale().Y;
					(*hFile) << itMesh.m_pkLightMap->GetCoordinateBias().X;
					(*hFile) << itMesh.m_pkLightMap->GetCoordinateBias().Y;

					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(2).X;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(2).Y;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(2).Z;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(2).W;

					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(2).X;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(2).Y;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(2).Z;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(2).W;

					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(3).X;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(3).Y;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(3).Z;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetScaleVector(3).W;

					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(3).X;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(3).Y;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(3).Z;
					(*hFile) << ((LightMap2DExt*)itMesh.m_pkLightMap)->GetAddVector(3).W;
				}
				else
				{
					(*hFile) << (uint32)0;
				}
				if (itMesh.m_pkShadowMap)
				{
					(*hFile) << (uint32)1;
					FVector4 kPenumbraSize = ((ShadowMap2DExt*)itMesh.m_pkShadowMap)->GetInvUniformPenumbraSize();
					(*hFile) << kPenumbraSize.X;
					(*hFile) << kPenumbraSize.Y;
					(*hFile) << kPenumbraSize.Z;
					(*hFile) << kPenumbraSize.W;
				}
				else
				{
					(*hFile) << (uint32)0;
				}
			}

			TArray<uint8> pixelBuffer;

			(*hFile) << m_kLandscape.m_i32ComponentSizeQuads;
			int32 i32CellPointNum = m_kLandscape.m_i32ComponentSizeQuads + 1;
			if (m_kLandscape.m_i32ComponentSizeQuads > 0)
			{
				(*hFile) << m_kLandscape.m_f32LightingResolution;
				Write(*hFile, m_kLandscape.m_strName);
				WritePosition(*hFile, m_kLandscape.m_v3Position);
				WriteScale(*hFile, m_kLandscape.m_v3Scale);
				(*hFile) << (uint32)m_kLandscape.m_mapUsedLayers.Num();
				for (auto& layer : m_kLandscape.m_mapUsedLayers)
				{
					(*hFile) << (int32)(layer.Key);
					(*hFile) << (float)(layer.Value.m_fTiling);
					(*hFile) << (float)(layer.Value.m_fYaw);
					(*hFile) << (float)(layer.Value.m_fPitch);
					(*hFile) << (float)(layer.Value.m_fSpecular);
					Write(*hFile, layer.Value.m_pkBase->GetName());
					Write(*hFile, layer.Value.m_pkNormal->GetName());
				}

				(*hFile) << (uint32)m_kLandscape.m_mapXYtoComponentMap.Num();
				for (auto& itComp : m_kLandscape.m_mapXYtoComponentMap)
				{
					(*hFile) << itComp.Get<0>().X;
					(*hFile) << itComp.Get<0>().Y;
					LandscapeComponent& comp = itComp.Get<1>();
					if (comp.m_pkLightMap)
					{
						(*hFile) << (uint32)1;
						Write(*hFile, ((LightMap2DExt*)comp.m_pkLightMap)->GetTexture(1)->GetName());
						(*hFile) << comp.m_pkLightMap->GetCoordinateScale().X;
						(*hFile) << comp.m_pkLightMap->GetCoordinateScale().Y;
						(*hFile) << comp.m_pkLightMap->GetCoordinateBias().X;
						(*hFile) << comp.m_pkLightMap->GetCoordinateBias().Y;

						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(2).X;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(2).Y;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(2).Z;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(2).W;

						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(2).X;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(2).Y;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(2).Z;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(2).W;

						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(3).X;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(3).Y;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(3).Z;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetScaleVector(3).W;

						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(3).X;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(3).Y;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(3).Z;
						(*hFile) << ((LightMap2DExt*)comp.m_pkLightMap)->GetAddVector(3).W;
					}
					else
					{
						(*hFile) << (uint32)0;
					}
					if (comp.m_pkShadowMap)
					{
						(*hFile) << (uint32)1;
						FVector4 kPenumbraSize = ((ShadowMap2DExt*)comp.m_pkShadowMap)->GetInvUniformPenumbraSize();
						(*hFile) << kPenumbraSize.X;
						(*hFile) << kPenumbraSize.Y;
						(*hFile) << kPenumbraSize.Z;
						(*hFile) << kPenumbraSize.W;
					}
					else
					{
						(*hFile) << (uint32)0;
					}

					int32 stride = comp.m_pkHeightMapNormal->GetSurfaceWidth() * 4;
					comp.m_pkHeightMapNormal->Source.GetMipData(pixelBuffer, 0);
					for (int32 i(0); i < i32CellPointNum; ++i)
					{
						int32 ptr = (comp.m_i32SectionBaseY + i) * stride + comp.m_i32SectionBaseX * 4;
						hFile->Write(&pixelBuffer[ptr], 4 * i32CellPointNum);
					}

					TArray<TArray<uint8>> weightMaps;
					weightMaps.SetNum(comp.m_aryLayerTextures.Num());
					for (int i(0); i < weightMaps.Num(); ++i)
					{
						comp.m_aryLayerTextures[i]->Source.GetMipData(weightMaps[i], 0);
					}

					(*hFile) << (int32)(comp.m_aryLayerInfo.Num());
					for (int32 i(0); i < comp.m_aryLayerInfo.Num(); ++i)
					{
						(*hFile) << (int32)(comp.m_aryLayerInfo[i].layerIdx);
					}

					for (int32 i(0); i < i32CellPointNum; ++i)
					{
						for (int32 j(0); j < i32CellPointNum; ++j)
						{
							for (int32 k(0); k < comp.m_aryLayerInfo.Num(); ++k)
							{
								(*hFile) << (uint8)weightMaps[comp.m_aryLayerInfo[k].texIdx][i32CellPointNum * i * 4 + j * 4 + comp.m_aryLayerInfo[k].channel];
							}
						}
					}
				}
			}

			TArray<const FVolumeLightingSample*> aryLightCacheSamples;
			for (auto tree : m_aryLightCacheData)
			{
				for (FLightVolumeOctree::TConstElementBoxIterator<> OctreeIt(*tree, tree->GetRootBounds());
					OctreeIt.HasPendingElements();
					OctreeIt.Advance())
				{
					const FVolumeLightingSample& VolumeSample = OctreeIt.GetCurrentElement();
					aryLightCacheSamples.Add(&VolumeSample);
				}
			}
			(*hFile) << (int32)aryLightCacheSamples.Num();
			for (int32 i(0); i < aryLightCacheSamples.Num(); ++i)
			{
				const FVolumeLightingSample& kSample = *aryLightCacheSamples[i];
				(*hFile) << (float)(-kSample.Position.X);
				(*hFile) << (float)(kSample.Position.Z);
				(*hFile) << (float)(kSample.Position.Y);
				(*hFile) << kSample.Radius;
				for (int32 j(0); j < 9; ++j)
				{
					(*hFile) << (float)kSample.Lighting.R.V[j];
					(*hFile) << (float)kSample.Lighting.G.V[j];
					(*hFile) << (float)kSample.Lighting.B.V[j];
				}
			}
			
			delete hFile;
			UE_LOG(SceneExporter, Log, TEXT("Level \"%s\" exported."), *kFileName);
		}
	}

	void ShowResult(EndState eEnd)
	{
		if (m_wpNotificationItem.IsValid())
		{
			TSharedPtr<SNotificationItem> spItem = m_wpNotificationItem.Pin();
			if (spItem.IsValid())
			{
				switch (eEnd)
				{
				case ES_SUCCEEDED:
					spItem->SetText(LOCTEXT("Exporting", "Exporting Scene Succeeded"));
					spItem->SetCompletionState(SNotificationItem::CS_Success);
					break;
				case ES_CANCELED:
					spItem->SetText(LOCTEXT("Exporting", "Exporting Scene Canceled"));
					spItem->SetCompletionState(SNotificationItem::CS_Fail);
					break;
				default:
					spItem->SetText(LOCTEXT("Exporting", "Exporting Scene Failed"));
					spItem->SetCompletionState(SNotificationItem::CS_Fail);
					break;
				}
				spItem->ExpireAndFadeout();
			}
		}
	}

	void Destory()
	{
		s_spProcess = nullptr;
	}

	void StopThread()
	{
		if (m_bIsRunning && (!m_bCanceling) && (!m_bExiting))
		{
			Cancel();
		}

		if (m_pkThread != nullptr)
		{
			m_pkThread->WaitForCompletion();
			delete m_pkThread;
			m_pkThread = nullptr;
		}
	}

	void StopMainTick()
	{
		if (m_hMainTick.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(m_hMainTick);
			m_hMainTick.Reset();
		}
	}

	FString m_kPath;
	FString m_kWorldName;

	FRunnableThread* m_pkThread = nullptr;
	FDelegateHandle m_hMainTick;
	FDateTime m_tStartTime = 0;
	FDateTime m_kEndTime = 0;
	float m_fSleepInterval = 0.0f;
	bool m_bIsRunning = false;
	bool m_bCanceling = false;
	bool m_bExiting = false;
	bool m_bFinished = false;

	TWeakPtr<SNotificationItem> m_wpNotificationItem;
	ring_buffer<std::function<void()>*, nullptr> m_kFGTasks;
	ring_buffer<std::function<void()>*, nullptr> m_kBGTasks;

	UExporter* m_pkMeshExporter = nullptr;
	UExporter* m_pkTGAExporter = nullptr;

	LandscapeInfo m_kLandscape;

	TMap<FString, int> m_mapInvolvedActorNames;
	TMap<FString, UStaticMesh*> m_mapFBXMeshes;
	TArray<StaticMeshInfo> m_aryStaticMeshes;
	TArray<ReflectionInfo> m_aryReflectionProbes;
	UDirectionalLightComponent* m_pkMainLight = nullptr;
	UExponentialHeightFogComponent* m_pkMainFog = nullptr;

	TMap<FString, LightMapInfo> m_mapLightMaps;
	TMap<FString, ShadowMapInfo> m_mapShadowMaps;
	TArray<FLightVolumeOctree*> m_aryLightCacheData;

};

void ExportTo(const FString& kPath)
{
	if (!s_spProcess.IsValid())
	{
		s_spProcess = MakeShareable(new ExportingProcess(kPath));
		FNotificationInfo kInfo(LOCTEXT("Exporting", "Exporting Scene..."));
		kInfo.Image = FEditorStyle::GetBrush(TEXT("MainFrame.PackageProject"));
		kInfo.bFireAndForget = false;
		kInfo.ExpireDuration = 2.0f;
		kInfo.Hyperlink = FSimpleDelegate::CreateStatic(ShowOutputLog);
		kInfo.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		TSharedPtr<SNotificationItem> spNotificationItem = FSlateNotificationManager::Get().AddNotification(kInfo);
		spNotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		s_spProcess->Launch(spNotificationItem);
	}
}
