//--------------------------------------------------------------------------------------
// File: MeshLoader.h
//
// Wrapper class for ID3DXMesh interface. Handles loading mesh data from an .mdl file
// and resource management for material textures.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include "optimize.h"//vtxfile header
#include "vtf.h"//vtffile header
using namespace OptimizedModel;
struct Vertex
{
	mstudiovertex_t	studiovertex;
	Vector4D vecTangent;
};
static ImageFormatInfo_t g_ImageFormatInfo[] =
{
	{ "IMAGE_FORMAT_RGBA8888",	4, 8, 8, 8, 8, false }, // IMAGE_FORMAT_RGBA8888,
	{ "IMAGE_FORMAT_ABGR8888",	4, 8, 8, 8, 8, false }, // IMAGE_FORMAT_ABGR8888, 
	{ "IMAGE_FORMAT_RGB888",	3, 8, 8, 8, 0, false }, // IMAGE_FORMAT_RGB888,
	{ "IMAGE_FORMAT_BGR888",	3, 8, 8, 8, 0, false }, // IMAGE_FORMAT_BGR888,
	{ "IMAGE_FORMAT_RGB565",	2, 5, 6, 5, 0, false }, // IMAGE_FORMAT_RGB565, 
	{ "IMAGE_FORMAT_I8",		1, 0, 0, 0, 0, false }, // IMAGE_FORMAT_I8,
	{ "IMAGE_FORMAT_IA88",		2, 0, 0, 0, 8, false }, // IMAGE_FORMAT_IA88
	{ "IMAGE_FORMAT_P8",		1, 0, 0, 0, 0, false }, // IMAGE_FORMAT_P8
	{ "IMAGE_FORMAT_A8",		1, 0, 0, 0, 8, false }, // IMAGE_FORMAT_A8
	{ "IMAGE_FORMAT_RGB888_BLUESCREEN", 3, 8, 8, 8, 0, false },	// IMAGE_FORMAT_RGB888_BLUESCREEN
	{ "IMAGE_FORMAT_BGR888_BLUESCREEN", 3, 8, 8, 8, 0, false },	// IMAGE_FORMAT_BGR888_BLUESCREEN
	{ "IMAGE_FORMAT_ARGB8888",	4, 8, 8, 8, 8, false }, // IMAGE_FORMAT_ARGB8888
	{ "IMAGE_FORMAT_BGRA8888",	4, 8, 8, 8, 8, false }, // IMAGE_FORMAT_BGRA8888
	{ "IMAGE_FORMAT_DXT1",		0, 0, 0, 0, 0, true }, // IMAGE_FORMAT_DXT1
	{ "IMAGE_FORMAT_DXT3",		0, 0, 0, 0, 8, true }, // IMAGE_FORMAT_DXT3
	{ "IMAGE_FORMAT_DXT5",		0, 0, 0, 0, 8, true }, // IMAGE_FORMAT_DXT5
	{ "IMAGE_FORMAT_BGRX8888",	4, 8, 8, 8, 0, false }, // IMAGE_FORMAT_BGRX8888
	{ "IMAGE_FORMAT_BGR565",	2, 5, 6, 5, 0, false }, // IMAGE_FORMAT_BGR565
	{ "IMAGE_FORMAT_BGRX5551",	2, 5, 5, 5, 0, false }, // IMAGE_FORMAT_BGRX5551
	{ "IMAGE_FORMAT_BGRA4444",	2, 4, 4, 4, 4, false },	 // IMAGE_FORMAT_BGRA4444
	{ "IMAGE_FORMAT_DXT1_ONEBITALPHA",		0, 0, 0, 0, 0, true }, // IMAGE_FORMAT_DXT1_ONEBITALPHA
	{ "IMAGE_FORMAT_BGRA5551",	2, 5, 5, 5, 1, false }, // IMAGE_FORMAT_BGRA5551
	{ "IMAGE_FORMAT_UV88",	    2, 8, 8, 0, 0, false }, // IMAGE_FORMAT_UV88
	{ "IMAGE_FORMAT_UVWQ8888",	    4, 8, 8, 8, 8, false }, // IMAGE_FORMAT_UV88
	{ "IMAGE_FORMAT_RGBA16161616F",	    8, 16, 16, 16, 16, false }, // IMAGE_FORMAT_UV88
};
inline ImageFormatInfo_t const& ImageFormatInfo( ImageFormat fmt )
{
	Assert( fmt < NUM_IMAGE_FORMATS );
	return g_ImageFormatInfo[fmt];
}
inline int SizeInBytes( ImageFormat fmt )
{
	return ImageFormatInfo(fmt).m_NumBytes;
}
inline int GetMemRequired( int width, int height, ImageFormat imageFormat, bool mipmap )
{
	if( !mipmap )
	{
		if( imageFormat == IMAGE_FORMAT_DXT1 ||
			imageFormat == IMAGE_FORMAT_DXT3 ||
			imageFormat == IMAGE_FORMAT_DXT5 )
		{
			Assert( ( width < 4 ) || !( width % 4 ) );
			Assert( ( height < 4 ) || !( height % 4 ) );
			if( width < 4 && width > 0 )
			{
				width = 4;
			}
			if( height < 4 && height > 0 )
			{
				height = 4;
			}
			int numBlocks = ( width * height ) >> 4;
			switch( imageFormat )
			{
			case IMAGE_FORMAT_DXT1:
				return numBlocks * 8;
				break;
			case IMAGE_FORMAT_DXT3:
			case IMAGE_FORMAT_DXT5:
				return numBlocks * 16;
				break;
			default:
				Assert( 0 );
				return 0;
				break;
			}
		}
		else
		{
			return width * height * SizeInBytes(imageFormat);
		}
	}
	else
	{
		int memSize = 0;

		while( 1 )
		{
			memSize += GetMemRequired( width, height, imageFormat, false );
			if( width == 1 && height == 1 )
			{
				break;
			}
			width >>= 1;
			height >>= 1;
			if( width < 1 )
			{
				width = 1;
			}
			if( height < 1 )
			{
				height = 1;
			}
		}

		return memSize;
	}
}
struct Material
{
    WCHAR strName[MAX_PATH];

    D3DXVECTOR3 vAmbient;
    D3DXVECTOR3 vDiffuse;
    D3DXVECTOR3 vSpecular;

    int nShininess;
    float fAlpha;

    bool bSpecular;

    WCHAR strTexture[MAX_PATH];
    IDirect3DTexture9* pTexture;
    D3DXHANDLE hTechnique;
};
enum ShaderName
{
	LightmappedGeneric = 0,
	SpriteCard = 1,
	UnlitGeneric = 2,
	UnlitTwoTexture = 3,
	VertexLitGeneric = 4,
	WorldTwoTextureBlend = 5,
	WorldVertexTransition = 6,

};
enum ShaderPropertyName
{
	additive,
	alpha,
	alphatest,
	basetexture,
	basetexturetransform,
	basetextureoffset,
	basetexturescale,
	basetexture2,
	basetexturetransform2,
	bumpbasetexture2withbumpmap,
	bumpmap,
	bumpscale,
	bumpframe,
	bumptransform,
	bumpoffset,
	bumpmap2,
	//bumpmapframe2,
	bumpframe2,
	nodiffusebumplighting,
	forcebump,

	color,
	decal,
	decalscale,
	detail,
	detailscale,
	detailframe,
	detail_alpha_mask_base_texture,
	detail2,
	detailscale2,
	envmap,
	envmapcontrast,
	envmapsaturation,
	envmaptint,
	envmapframe,
	envmapmode,
	envmapsphere,
	basetexturenoenvmap,
	basetexture2noenvmap,
	envmapoptional,
	envmapmask,
	halflambert,
	model,
	nocull,
	parallaxmap,
	parallaxmapscale,
	phong,
	phongexponenttexture,
	phongexponent,
	phongboost,
	phongfresnelranges,
	lightwarptexture,
	phongalbedotint,
	ambientocclusiontexture,
	selfillum,
	selfillumtint,
	surfaceprop,
	translucent,
	writeZ,
	END,
};
struct Property
{
	ShaderPropertyName name;
	WCHAR strValue[MAX_PATH];
};
struct ShaderInfo
{
    ShaderName name;
	CGrowableArray< Property > propertis;
};
class CMeshLoader
{
public:
    CMeshLoader();
    ~CMeshLoader();

    HRESULT Create( IDirect3DDevice9* pd3dDevice, const WCHAR* strFileName );
    void    Destroy();
    
    
    UINT GetNumMaterials() const { return m_Materials.GetSize(); }
    Material* GetMaterial( UINT iMaterial ) { return m_Materials.GetAt( iMaterial ); }

    ID3DXMesh* GetMesh() { return m_pMesh; }
    WCHAR* GetMediaDirectory() { return m_strMediaDir; }
	HRESULT CreateTextureFromVTF( IDirect3DDevice9* pd3dDevice, const WCHAR* strFilename,  IDirect3DTexture9** ppTexture );
	HRESULT GetMaterialFromVMT( const char* strFileName, ShaderInfo*  pShaderInfo  );
private:
    
    HRESULT LoadGeometryFromMDL( const WCHAR* strFileName );

    void    InitMaterial( Material* pMaterial );
    
    //DWORD   AddVertex( UINT hash, VERTEX* pVertex );
    //void    DeleteCache();

    IDirect3DDevice9* m_pd3dDevice;    // Direct3D Device object associated with this mesh
    ID3DXMesh*        m_pMesh;         // Encapsulated D3DX Mesh
	unsigned short    m_iLod;
	vertexFileHeader_t* m_pVvdFileHeader;
	FileHeader_t*	 m_pVtxFileHeader;
	studiohdr_t*	 m_pMdlFileHeader;
    CGrowableArray< Vertex >      m_Vertices;      // Filled and copied to the vertex buffer
    CGrowableArray< Material* >   m_Materials;     // Holds material properties per subset
	CGrowableArray< DWORD >       m_Attributes;    // Filled and copied to the attribute buffer
    CGrowableArray< unsigned short >       m_Indices;       // Filled and copied to the index buffer
    WCHAR m_strMediaDir[ MAX_PATH ];               // Directory where the mesh was found
};
