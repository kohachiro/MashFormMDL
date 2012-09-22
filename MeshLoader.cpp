//--------------------------------------------------------------------------------------
// File: MeshLoader.cpp
//
// Wrapper class for ID3DXMesh interface. Handles loading mesh data from an .obj file
// and resource management for material textures.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#define DXUT_AUTOLIB
#include "DXUT.h"
#include "SDKmisc.h"
#pragma warning(disable: 4995)
#include "meshloader.h"
#include <fstream>
using namespace std;
#pragma warning(default: 4995)


// Vertex declaration
D3DVERTEXELEMENT9 VERTEX_DECL[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_BLENDWEIGHT,  0},
    { 0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_BLENDINDICES, 0},
    { 0, 16, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
    { 0, 28, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0}, 
    { 0, 40, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
    { 0, 48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TANGENT,  0},
	D3DDECL_END()
};


//--------------------------------------------------------------------------------------
CMeshLoader::CMeshLoader()
{
    m_pd3dDevice = NULL;  
    m_pMesh = NULL;  
	m_iLod = 0;
    ZeroMemory( m_strMediaDir, sizeof(m_strMediaDir) );
}


//--------------------------------------------------------------------------------------
CMeshLoader::~CMeshLoader()
{
    Destroy();
}


//--------------------------------------------------------------------------------------
void CMeshLoader::Destroy()
{
    for( int iMaterial=0; iMaterial < m_Materials.GetSize(); iMaterial++ )
    {
        Material* pMaterial = m_Materials.GetAt( iMaterial );
        
        // Avoid releasing the same texture twice
        for( int x=iMaterial+1; x < m_Materials.GetSize(); x++ )
        {
            Material* pCur = m_Materials.GetAt( x );
            if( pCur->pTexture == pMaterial->pTexture )
                pCur->pTexture = NULL;
        }

        SAFE_RELEASE( pMaterial->pTexture );
        SAFE_DELETE( pMaterial );
    }

    m_Materials.RemoveAll();
    m_Vertices.RemoveAll();
    m_Attributes.RemoveAll();
	
    SAFE_RELEASE( m_pMesh );
	SAFE_DELETE( m_pVvdFileHeader );
	SAFE_DELETE( m_pVtxFileHeader );
	SAFE_DELETE( m_pMdlFileHeader );

    m_pd3dDevice = NULL;
}


//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::Create( IDirect3DDevice9* pd3dDevice, const WCHAR* strFilename )
{
    HRESULT hr;
    WCHAR str[ MAX_PATH ] = {0};

    // Start clean
    Destroy();

    // Store the device pointer
    m_pd3dDevice = pd3dDevice;

    // Load the vertex buffer, index buffer, and subset information from a file. In this case, 
    // an .obj file was chosen for simplicity, but it's meant to illustrate that ID3DXMesh objects
    // can be filled from any mesh file format once the necessary data is extracted from file.
    V_RETURN( LoadGeometryFromMDL( strFilename ) );

    // Set the current directory based on where the mesh was found
    WCHAR wstrOldDir[MAX_PATH] = {0};
    GetCurrentDirectory( MAX_PATH, wstrOldDir );
    SetCurrentDirectory( m_strMediaDir );

    // Load material textures
    for( int iMaterial=0; iMaterial < m_Materials.GetSize(); iMaterial++ )
    {
        Material* pMaterial = m_Materials.GetAt( iMaterial );
        if( pMaterial->strTexture[0] )
        {   
            // Avoid loading the same texture twice
            bool bFound = false;
            for( int x=0; x < iMaterial; x++ )
            {
                Material* pCur = m_Materials.GetAt( x );
                if( 0 == wcscmp( pCur->strTexture, pMaterial->strTexture ) )
                {
                    bFound = true;
                    pMaterial->pTexture = pCur->pTexture;
                    break;
                }
            }

            // Not found, load the texture
            if( !bFound )
            {
                V_RETURN( CreateTextureFromVTF( pd3dDevice, pMaterial->strTexture, &(pMaterial->pTexture) ) );
            }
        }
    }

    // Restore the original current directory
    SetCurrentDirectory( wstrOldDir );

    // Create the encapsulated mesh
    ID3DXMesh* pMesh = NULL;
	//StripGroupHeader_t* pStripGroup= m_pVtxFileHeader->pBodyPart(0)->pModel(0)->pLOD(m_iLod)->pMesh(0)->pStripGroup(0);
	V_RETURN( D3DXCreateMesh( m_Indices.GetSize() / 3,m_Vertices.GetSize(), 
                              D3DXMESH_MANAGED, VERTEX_DECL, 
                              pd3dDevice, &pMesh ) ); 
    // Copy the vertex data
    mstudiovertex_t* pVertex;
    V_RETURN( pMesh->LockVertexBuffer( 0, (void**) &pVertex ) );
    memcpy( pVertex, m_Vertices.GetData(), m_Vertices.GetSize() * sizeof( Vertex ) );
    pMesh->UnlockVertexBuffer();
    m_Vertices.RemoveAll();
    
    //Copy the index data
    unsigned short * pIndex;
    V_RETURN( pMesh->LockIndexBuffer( 0, (void**) &pIndex ) );
	memcpy( pIndex, m_Indices.GetData(), m_Indices.GetSize() * sizeof( unsigned short ) );
    pMesh->UnlockIndexBuffer();
    m_Indices.RemoveAll();

    // Copy the attribute data
    DWORD * pSubset;
    V_RETURN( pMesh->LockAttributeBuffer( 0, &pSubset ) );
    memcpy( pSubset, m_Attributes.GetData(), m_Attributes.GetSize() * sizeof( DWORD ) );
    pMesh->UnlockAttributeBuffer();
    m_Attributes.RemoveAll();

    m_pMesh = pMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::LoadGeometryFromMDL( const WCHAR* strFileName )
{
    WCHAR strMaterialFilename[MAX_PATH] = {0};
	char strMaterial[MAX_PATH]={0};
    HRESULT hr;
    
	WCHAR vvdwstr[MAX_PATH];
	WCHAR vtxwstr[MAX_PATH];
	WCHAR mdlwstr[MAX_PATH];

	char vvdstr[MAX_PATH];
	char vtxstr[MAX_PATH];
	char mdlstr[MAX_PATH];

	int vvdFileSize;
	int vtxFileSize;
	int mdlFileSize;

	StringCchCopy( vvdwstr, MAX_PATH, strFileName );
	StringCchCopy( vtxwstr, MAX_PATH, strFileName );
	StringCchCopy( mdlwstr, MAX_PATH, strFileName );

	StringCchCat( vvdwstr, MAX_PATH, L".vvd" );
	StringCchCat( vtxwstr, MAX_PATH, L".dx90.vtx" );
	StringCchCat( mdlwstr, MAX_PATH, L".mdl" );

    // Find the file
	V_RETURN( DXUTFindDXSDKMediaFileCch( vvdwstr, MAX_PATH, vvdwstr ) );
	V_RETURN( DXUTFindDXSDKMediaFileCch( vtxwstr, MAX_PATH, vtxwstr ) );
	V_RETURN( DXUTFindDXSDKMediaFileCch( mdlwstr, MAX_PATH, mdlwstr ) );


	WideCharToMultiByte( CP_ACP, 0, vvdwstr, -1, vvdstr, MAX_PATH, NULL, NULL );
	WideCharToMultiByte( CP_ACP, 0, vtxwstr, -1, vtxstr, MAX_PATH, NULL, NULL );
	WideCharToMultiByte( CP_ACP, 0, mdlwstr, -1, mdlstr, MAX_PATH, NULL, NULL );

	ifstream vvdFile( vvdstr,ios::binary );
	ifstream vtxFile( vtxstr,ios::binary );
	ifstream mdlFile( mdlstr,ios::binary );

	vvdFile.seekg(0,ios::end);
	vtxFile.seekg(0,ios::end);
	mdlFile.seekg(0,ios::end);

	vvdFileSize   =   vvdFile.tellg();
	vtxFileSize   =   vtxFile.tellg();
	mdlFileSize   =   mdlFile.tellg();

	vvdFile.seekg(0,ios::beg);
	vtxFile.seekg(0,ios::beg);
	mdlFile.seekg(0,ios::beg);

	char *m_pVvdMem = new char[vvdFileSize];
	char *m_pVtxMem = new char[vtxFileSize];
	char *m_pMdlMem = new char[mdlFileSize];

	vvdFile.read(m_pVvdMem,vvdFileSize);
	vtxFile.read(m_pVtxMem,vtxFileSize);
	mdlFile.read(m_pMdlMem,mdlFileSize);

	m_pVvdFileHeader=(vertexFileHeader_t *)m_pVvdMem;
	m_pVtxFileHeader=(FileHeader_t *)m_pVtxMem;
	m_pMdlFileHeader=(studiohdr_t *)m_pMdlMem;

	if (m_pVvdFileHeader->numFixups)
	{
		vertexFileHeader_t *	pNewVvdHdr;
		pNewVvdHdr = new vertexFileHeader_t[vvdFileSize];
		Studio_LoadVertexes( m_pVvdFileHeader, pNewVvdHdr, 0, true );
		delete m_pVvdFileHeader;
		m_pVvdFileHeader = pNewVvdHdr;
	}
	// check mdl header
	if (m_pMdlFileHeader->id != IDSTUDIOHEADER)
	{
		MessageBox(NULL,L".mdl File id error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}
	if (m_pMdlFileHeader->version != STUDIO_VERSION)
	{
		MessageBox(NULL,L".mdl File version error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}

	// check vtx header
	if (m_pVtxFileHeader->version != OPTIMIZED_MODEL_FILE_VERSION)
	{
		MessageBox(NULL,L".vtd File version error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}
	if (m_pVtxFileHeader->checkSum != m_pMdlFileHeader->checksum)
	{
		MessageBox(NULL,L".vtd File checksum error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}

	// check vvd header
	if (m_pVvdFileHeader->id != MODEL_VERTEX_FILE_ID)
	{
		MessageBox(NULL,L".vvd File id error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}
	if (m_pVvdFileHeader->version != MODEL_VERTEX_FILE_VERSION)
	{
		MessageBox(NULL,L".vvd File version error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}
	if (m_pVvdFileHeader->checksum != m_pMdlFileHeader->checksum)
	{
		MessageBox(NULL,L".vvd File checksum error",NULL, MB_OK | MB_ICONERROR);
		return S_FALSE;
	}

	m_pMdlFileHeader->pVertexBase = (void *)m_pVvdFileHeader;
	m_pMdlFileHeader->pIndexBase  = (void *)m_pVtxFileHeader;

	BodyPartHeader_t* pBodyPart = m_pVtxFileHeader->pBodyPart(0);
	ModelHeader_t*  pModel=pBodyPart->pModel(0);
	ModelLODHeader_t* pLod = pModel->pLOD(m_iLod);

	mstudiobodyparts_t* pStudioBodyPart = m_pMdlFileHeader->pBodypart(0);
	mstudiomodel_t* pStudioModel= pStudioBodyPart->pModel(0);
	unsigned short indexOffset=0;
	unsigned short iSubset = 0;
	for (int k=0;k<pStudioModel->nummeshes;k++)
	{
		MeshHeader_t* pMesh = pLod->pMesh(k);
		mstudiomesh_t* pStudioMesh = pStudioModel->pMesh( k );
		for (int j=0;j<pMesh->numStripGroups;j++)
		{
			StripGroupHeader_t* pStripGroup = pMesh->pStripGroup(j);
			for (int i=0;i<pStripGroup->numVerts;i++)
			{
				Vertex vertex;
				mstudiovertex_t studiovertex=* m_pVvdFileHeader->pVertex( pStudioMesh->vertexoffset+pStripGroup->pVertex(i)->origMeshVertID );
				vertex.studiovertex = studiovertex;
				vertex.vecTangent = * m_pVvdFileHeader->pTangent(pStripGroup->pVertex(i)->origMeshVertID );
				m_Vertices.Add(vertex);
			}
			for (int i=0;i<pStripGroup->numIndices;i+=3)
			{
				m_Indices.Add(indexOffset + *pStripGroup->pIndex(i));
				m_Indices.Add(indexOffset + *pStripGroup->pIndex(i+1));
				m_Indices.Add(indexOffset + *pStripGroup->pIndex(i+2));
				m_Attributes.Add( iSubset );
			}
			indexOffset=m_Vertices.GetSize();
			iSubset++;
		}
	}

	for (int i=0;i<	m_pMdlFileHeader->numtextures;i++)
	{
		char * pSkinName = m_pMdlFileHeader->pTexture(i)->pszName();
		char * pPathName = m_pMdlFileHeader->pCdtexture(i);
		char pTempName[MAX_PATH];
		StringCchCopyA(pTempName,MAX_PATH,pPathName);
		StringCchCatA(pTempName,MAX_PATH,pSkinName);
		ShaderInfo shaderInfo;
		GetMaterialFromVMT( pTempName, &shaderInfo );
		Material* pMaterial = new Material();
		InitMaterial( pMaterial );
		WCHAR strName[MAX_PATH];
		StringCchCopy( pMaterial->strTexture, MAX_PATH, shaderInfo.propertis.GetAt((int)ShaderPropertyName::basetexture).strValue );
		StringCchCopy( pMaterial->strName, MAX_PATH, pMaterial->strTexture );
		StringCchCat( pMaterial->strTexture, MAX_PATH, L".vtf" );
		m_Materials.Add( pMaterial );
	}

	//WCHAR   string[25];
	//_itow_s(m_Indices.GetSize(),   string,   10); 
	//MessageBox(NULL,string,NULL, MB_OK | MB_ICONERROR);

    // Cleanup
	vvdFile.close();
	vtxFile.close();
	mdlFile.close();

	return S_OK;
}

//--------------------------------------------------------------------------------------
void CMeshLoader::InitMaterial( Material* pMaterial )
{
    ZeroMemory( pMaterial, sizeof(Material) );

    pMaterial->vAmbient = D3DXVECTOR3(0.2f, 0.2f, 0.2f);
    pMaterial->vDiffuse = D3DXVECTOR3(0.8f, 0.8f, 0.8f);
    pMaterial->vSpecular = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
    pMaterial->nShininess = 0;
    pMaterial->fAlpha = 1.0f;
    pMaterial->bSpecular = false;
    pMaterial->pTexture = NULL;
}
//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::CreateTextureFromVTF( IDirect3DDevice9* pd3dDevice, const WCHAR* strFilename,  IDirect3DTexture9** ppTexture )
{
    HRESULT hr;
	WCHAR wstr[MAX_PATH]={0};
	char str[MAX_PATH]={0};
	int fileSize;
    V_RETURN( DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strFilename ) );
	WideCharToMultiByte( CP_ACP, 0, wstr, -1, str, MAX_PATH, NULL, NULL );

	ifstream inFile( str,ios::binary );
	inFile.seekg(0,ios::end);
	fileSize   =   inFile.tellg();
	inFile.seekg(0,ios::beg);
	char *pMem = new char[fileSize];
	inFile.read(pMem,fileSize);

	VTFFileHeader_t* pVtf=(VTFFileHeader_t *)pMem;
	D3DFORMAT format=ImageFormatToD3DFormat(pVtf->imageFormat);

	//D3DLOCKED_RECT rectD3D;
	//V_RETURN( pd3dDevice->CreateTexture( pVtf->width, pVtf->height, 0, D3DUSAGE_DYNAMIC|D3DUSAGE_AUTOGENMIPMAP, format, D3DPOOL_DEFAULT, ppTexture, NULL ) );
	//V_RETURN((*ppTexture)->LockRect( 0, &rectD3D, NULL, 0 ));
	//unsigned char* pRect = (unsigned char*) rectD3D.pBits; 
	//int memRequired = GetMemRequired( pVtf->width, pVtf->height, pVtf->imageFormat, false );
	//int memOffset = GetMemRequired( pVtf->width, pVtf->height, pVtf->imageFormat, true ) - memRequired;
	//int ThumbnailBufferSize = GetMemRequired( pVtf->lowResImageWidth, pVtf->lowResImageHeight, pVtf->lowResImageFormat, false );
	//int ImageDataOffset = pVtf->headerSize + ThumbnailBufferSize;
	//unsigned char* pSrc  = (unsigned char*)(((unsigned char*)pVtf) + ImageDataOffset + memOffset );	
	//memcpy( pRect, pSrc, memRequired );
	//V_RETURN((*ppTexture)->UnlockRect( 0 ));
	//(*ppTexture)->GenerateMipSubLevels();
	//int x=(*ppTexture)->GetLevelCount();

	V_RETURN( pd3dDevice->CreateTexture( pVtf->width, pVtf->height, pVtf->numMipLevels, D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT, ppTexture, NULL ) );
	int ThumbnailBufferSize = GetMemRequired( pVtf->lowResImageWidth, pVtf->lowResImageHeight, pVtf->lowResImageFormat, false );
	int ImageDataOffset = pVtf->headerSize + ThumbnailBufferSize;
	int size=1;
	int offset=0;
	int memRequired=0;
	unsigned char* pRect;
	unsigned char* pSrc;
	D3DLOCKED_RECT rectD3D;
	for (int i=0;i< pVtf->numMipLevels; i++)
	{
		V_RETURN((*ppTexture)->LockRect( pVtf->numMipLevels-1-i, &rectD3D, NULL, 0 ));
		pRect = (unsigned char*) rectD3D.pBits; 
		pSrc  = (unsigned char*)(((unsigned char*)pVtf) + ImageDataOffset + offset );	
		memRequired = GetMemRequired( size, size, pVtf->imageFormat, false );
		memcpy( pRect, pSrc, memRequired );
		V_RETURN((*ppTexture)->UnlockRect( pVtf->numMipLevels-1-i ));
		size*=2;
		offset+=memRequired;
	}
	delete pMem;
	inFile.close();
	return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CMeshLoader::GetMaterialFromVMT( const char* strFileName ,ShaderInfo*  pShaderInfo )
{
	char materialName[MAX_PATH];
	StringCchCopyA( materialName, MAX_PATH, strFileName );
	StringCchCatA( materialName, MAX_PATH, ".vmt" );
	
    WCHAR strTemp[MAX_PATH] = {0};
	WCHAR strValue[MAX_PATH] = {0};
	Property tProperty;
	for ( int i=0;i<ShaderPropertyName::END;i++ )
		pShaderInfo->propertis.Add(tProperty);	
	wifstream InFile( materialName );
	bool addParameter = false;
    if( !InFile )
        return DXTRACE_ERR( L"wifstream::open", E_FAIL );
    for(;;)
    {
        InFile >> strTemp;
        if( !InFile )
            break;
        if( 0 == _wcsicmp( strTemp, L"\"LightmappedGeneric\"" ) )
        {
			pShaderInfo->name = ShaderName::LightmappedGeneric;
		}
        else if( 0 == _wcsicmp( strTemp, L"\"UnlitGeneric\"" ) )
        {
			pShaderInfo->name = ShaderName::UnlitGeneric;
		}
        else if( 0 == _wcsicmp( strTemp, L"\"UnlitTwoTexture\"" ) )
        {
			pShaderInfo->name = ShaderName::UnlitTwoTexture;
		}
        else if( 0 == _wcsicmp( strTemp, L"\"VertexLitGeneric\"" ) )
        {
			pShaderInfo->name = ShaderName::VertexLitGeneric;
		}
        else if( 0 == _wcsicmp( strTemp, L"\"WorldTwoTextureBlend\"" ) )
        {
			pShaderInfo->name = ShaderName::WorldTwoTextureBlend;
		}
        else if( 0 == _wcsicmp( strTemp, L"\"WorldVertexTransition\"" ) )
        {
			pShaderInfo->name = ShaderName::WorldVertexTransition;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$additive\"" ) )
		{
			tProperty.name = ShaderPropertyName::additive;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$alpha\"" ) )
		{
			tProperty.name = ShaderPropertyName::alpha;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$alphatest\"" ) )
		{
			tProperty.name = ShaderPropertyName::alphatest;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexture\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexture;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexturetransform\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexturetransform;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetextureoffset\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetextureoffset;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexturescale\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexturescale;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexture2\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexture2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexturetransform2\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexturetransform2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpbasetexture2withbumpmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpbasetexture2withbumpmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpscale\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpscale;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpframe\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpframe;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumptransform\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumptransform;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpoffset\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpoffset;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpmap2\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpmap2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$bumpframe2\"" ) )
		{
			tProperty.name = ShaderPropertyName::bumpframe2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$nodiffusebumplighting\"" ) )
		{
			tProperty.name = ShaderPropertyName::nodiffusebumplighting;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$forcebump\"" ) )
		{
			tProperty.name = ShaderPropertyName::forcebump;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$color\"" ) )
		{
			tProperty.name = ShaderPropertyName::color;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$decal\"" ) )
		{
			tProperty.name = ShaderPropertyName::decal;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$decalscale\"" ) )
		{
			tProperty.name = ShaderPropertyName::decalscale;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detail\"" ) )
		{
			tProperty.name = ShaderPropertyName::detail;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detailscale\"" ) )
		{
			tProperty.name = ShaderPropertyName::detailscale;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detailframe\"" ) )
		{
			tProperty.name = ShaderPropertyName::detailframe;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detail_alpha_mask_base_texture\"" ) )
		{
			tProperty.name = ShaderPropertyName::detail_alpha_mask_base_texture;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detail2\"" ) )
		{
			tProperty.name = ShaderPropertyName::detail2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$detailscale2\"" ) )
		{
			tProperty.name = ShaderPropertyName::detailscale2;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapcontrast\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapcontrast;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapsaturation\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapsaturation;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmaptint\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmaptint;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapframe\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapframe;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapmode\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapmode;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapsphere\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapsphere;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$basetexturenoenvmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexturenoenvmap;
			addParameter = true;
		}

		else if( 0 == _wcsicmp( strTemp, L"\"$basetexture2noenvmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::basetexture2noenvmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapoptional\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapoptional;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$envmapmask\"" ) )
		{
			tProperty.name = ShaderPropertyName::envmapmask;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$halflambert\"" ) )
		{
			tProperty.name = ShaderPropertyName::halflambert;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$model\"" ) )
		{
			tProperty.name = ShaderPropertyName::model;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$nocull\"" ) )
		{
			tProperty.name = ShaderPropertyName::nocull;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$parallaxmap\"" ) )
		{
			tProperty.name = ShaderPropertyName::parallaxmap;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$parallaxmapscale\"" ) )
		{
			tProperty.name = ShaderPropertyName::parallaxmapscale;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phong\"" ) )
		{
			tProperty.name = ShaderPropertyName::phong;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phongexponenttexture\"" ) )
		{
			tProperty.name = ShaderPropertyName::phongexponenttexture;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phongexponent\"" ) )
		{
			tProperty.name = ShaderPropertyName::phongexponent;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phongboost\"" ) )
		{
			tProperty.name = ShaderPropertyName::phongboost;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phongfresnelranges\"" ) )
		{
			tProperty.name = ShaderPropertyName::phongfresnelranges;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$lightwarptexture\"" ) )
		{
			tProperty.name = ShaderPropertyName::lightwarptexture;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$phongalbedotint\"" ) )
		{
			tProperty.name = ShaderPropertyName::phongalbedotint;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$ambientocclusiontexture\"" ) )
		{
			tProperty.name = ShaderPropertyName::ambientocclusiontexture;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$selfillum\"" ) )
		{
			tProperty.name = ShaderPropertyName::selfillum;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$selfillumtint\"" ) )
		{
			tProperty.name = ShaderPropertyName::selfillumtint;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$surfaceprop\"" ) )
		{
			tProperty.name = ShaderPropertyName::surfaceprop;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$translucent\"" ) )
		{
			tProperty.name = ShaderPropertyName::translucent;
			addParameter = true;
		}
		else if( 0 == _wcsicmp( strTemp, L"\"$writeZ\"" ) )
		{
			tProperty.name = ShaderPropertyName::writeZ;
			addParameter = true;
		}
		if ( addParameter )
		{
			InFile >> strValue;
			if( wcschr( strValue, '"' ) != NULL )
			{
				WCHAR* pch = wcsrchr( strValue, L'"' );
				*pch='\0';
				StringCchCopy( tProperty.strValue, MAX_PATH, &strValue[1] );
			}
			else
			{
				StringCchCopy( tProperty.strValue, MAX_PATH, strValue );
			}
			pShaderInfo->propertis.SetAt( tProperty.name, tProperty );
			addParameter = false;
		}
	}
    return S_OK;
}

