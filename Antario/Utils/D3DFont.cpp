//-----------------------------------------------------------------------------
// File: D3DFont.cpp
//
// Desc: Texture-based font class
//-----------------------------------------------------------------------------

#pragma once
#include <stdio.h>
#include <cmath>
#include <tchar.h>

#include "D3DFont.h"




//-----------------------------------------------------------------------------
// Custom vertex types for rendering text
//-----------------------------------------------------------------------------
#define MAX_NUM_VERTICES 50*6

struct FONT2DVERTEX
{
    D3DXVECTOR4 p;
    DWORD       color;
    FLOAT       tu, tv;
};

struct FONT3DVERTEX
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
    FLOAT       tu, tv;
};

#define D3DFVF_FONT2DVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)
#define D3DFVF_FONT3DVERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)

inline FONT2DVERTEX InitFont2DVertex(const D3DXVECTOR4& p, D3DCOLOR color,
                                     FLOAT              tu, FLOAT   tv)
{
    FONT2DVERTEX v;
    v.p     = p;
    v.color = color;
    v.tu    = tu;
    v.tv    = tv;
    return v;
}

inline FONT3DVERTEX InitFont3DVertex(const D3DXVECTOR3& p, const D3DXVECTOR3& n,
                                     FLOAT              tu, FLOAT             tv)
{
    FONT3DVERTEX v;
    v.p  = p;
    v.n  = n;
    v.tu = tu;
    v.tv = tv;
    return v;
}




//-----------------------------------------------------------------------------
// Name: CD3DFont()
// Desc: Font class constructor
//-----------------------------------------------------------------------------
CD3DFont::CD3DFont(const TCHAR* strFontName, DWORD dwHeight, DWORD dwWeight, DWORD dwFlags)
{
    wcsncpy_s(this->strFontName, strFontName, sizeof(this->strFontName) / sizeof(TCHAR));
    this->strFontName[sizeof(this->strFontName) / sizeof(TCHAR) - 1] = _T('\0');
    this->dwFontHeight      = dwHeight;
    this->dwFontWeight      = dwWeight;
    this->dwFontFlags       = dwFlags;
    this->dwSpacing         = 0;

    this->pd3dDevice = NULL;
    this->pTexture   = NULL;
    this->pVB        = NULL;

    this->pStateBlockSaved      = NULL;
    this->pStateBlockDrawString = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CD3DFont()
// Desc: Font class destructor
//-----------------------------------------------------------------------------
CD3DFont::~CD3DFont()
{
    InvalidateDeviceObjects();
    DeleteDeviceObjects();
}




//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Initializes device-dependent objects, including the vertex buffer used
//       for rendering text and the texture map which stores the font image.
//-----------------------------------------------------------------------------
HRESULT CD3DFont::InitDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice)
{
    HRESULT hr;

    // Keep a local copy of the device
    this->pd3dDevice = pd3dDevice;

    // Establish the font and texture size
    this->fTextScale = 1.0f; // Draw fonts into texture without scaling

    // Large fonts need larger textures
    if (this->dwFontHeight > 60)
        this->dwTexWidth = this->dwTexHeight = 2048;
    else if (this->dwFontHeight > 30)
        this->dwTexWidth = this->dwTexHeight = 1024;
    else if (this->dwFontHeight > 15)
        this->dwTexWidth = this->dwTexHeight = 512;
    else
        this->dwTexWidth = this->dwTexHeight = 256;

    // If requested texture is too big, use a smaller texture and smaller font,
    // and scale up when rendering.
    D3DCAPS9 d3dCaps;
    this->pd3dDevice->GetDeviceCaps(&d3dCaps);

    if (this->dwTexWidth > d3dCaps.MaxTextureWidth)
    {
        this->fTextScale = (FLOAT)d3dCaps.MaxTextureWidth / (FLOAT)this->dwTexWidth;
        this->dwTexWidth = this->dwTexHeight = d3dCaps.MaxTextureWidth;
    }

    // Create a new texture for the font
    hr = this->pd3dDevice->CreateTexture(this->dwTexWidth, this->dwTexHeight, 1,
                                         0, D3DFMT_A4R4G4B4,
                                         D3DPOOL_MANAGED, &this->pTexture, NULL);
    if (FAILED(hr))
        return hr;

    // Prepare to create a bitmap
    DWORD*     pBitmapBits;
    BITMAPINFO bmi;
    ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = (int)this->dwTexWidth;
    bmi.bmiHeader.biHeight      = -(int)this->dwTexHeight;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biBitCount    = 32;

    // Create a DC and a bitmap for the font
    HDC     hDC       = CreateCompatibleDC(NULL);
    HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS,
                                         (void**)&pBitmapBits, NULL, 0);

    // Sanity checks
    if (hDC == NULL)
        return E_FAIL;
    if (hbmBitmap == NULL)
        return E_FAIL;

    SetMapMode(hDC, MM_TEXT);

    // Create a font.  By specifying ANTIALIASED_QUALITY, we might get an
    // antialiased font, but this is not guaranteed.
    INT   nHeight  = -MulDiv(this->dwFontHeight, (INT)(GetDeviceCaps(hDC, LOGPIXELSY) * this->fTextScale), 72);
    DWORD dwItalic = (this->dwFontFlags & D3DFONT_ITALIC) ? TRUE : FALSE;
    HFONT hFont    = CreateFont(nHeight, 0, 0, 0, this->dwFontWeight, dwItalic,
                                FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                this->dwFontHeight > 8 ? CLEARTYPE_NATURAL_QUALITY : ANTIALIASED_QUALITY,
                                VARIABLE_PITCH, this->strFontName);

    if (NULL == hFont)
        return E_FAIL;

    HGDIOBJ hbmOld   = SelectObject(hDC, hbmBitmap);
    HGDIOBJ hFontOld = SelectObject(hDC, hFont);

    // Set text properties
    SetTextColor(hDC, RGB(255, 255, 255));
    SetBkColor(hDC, 0x00000000);
    SetTextAlign(hDC, TA_TOP);

    // Loop through all printable character and output them to the bitmap..
    // Meanwhile, keep track of the corresponding tex coords for each character.
    DWORD x      = 0;
    DWORD y      = 0;
    TCHAR str[2] = _T("x");
    SIZE  size;

    // Calculate the spacing between characters based on line height
    GetTextExtentPoint32(hDC, TEXT(" "), 1, &size);
    x = this->dwSpacing = (DWORD)ceil(size.cy * 0.3f);

    for (TCHAR c = 32; c < 127; c++)
    {
        str[0] = c;
        GetTextExtentPoint32(hDC, str, 1, &size);

        if ((DWORD)(x + size.cx + this->dwSpacing) > this->dwTexWidth)
        {
            x = this->dwSpacing;
            y += size.cy + 1;
        }

        ExtTextOut(hDC, x + 0, y + 0, ETO_OPAQUE, NULL, str, 1, NULL);

        this->fTexCoords[c - 32][0] = ((FLOAT)(x + 0 - this->dwSpacing)) / this->dwTexWidth;
        this->fTexCoords[c - 32][1] = ((FLOAT)(y + 0 + 0)) / this->dwTexHeight;
        this->fTexCoords[c - 32][2] = ((FLOAT)(x + size.cx + this->dwSpacing)) / this->dwTexWidth;
        this->fTexCoords[c - 32][3] = ((FLOAT)(y + size.cy + 0)) / this->dwTexHeight;

        x += size.cx + (2 * this->dwSpacing);
    }

    // Lock the surface and write the alpha values for the set pixels
    D3DLOCKED_RECT d3dlr;
    this->pTexture->LockRect(0, &d3dlr, 0, 0);
    BYTE* pDstRow = (BYTE*)d3dlr.pBits;
    WORD* pDst16;
    BYTE  bAlpha; // 4-bit measure of pixel intensity

    for (y = 0; y < this->dwTexHeight; y++)
    {
        pDst16 = (WORD*)pDstRow;
        for (x = 0; x < this->dwTexWidth; x++)
        {
            bAlpha = (BYTE)((pBitmapBits[this->dwTexWidth * y + x] & 0xff) >> 4);
            if (bAlpha > 0)
            {
                *pDst16++ = (WORD)((bAlpha << 12) | 0x0fff);
            }
            else
            {
                *pDst16++ = 0x0000;
            }
        }
        pDstRow += d3dlr.Pitch;
    }

    // Done updating texture, so clean up used objects
    this->pTexture->UnlockRect(0);
    SelectObject(hDC, hbmOld);
    SelectObject(hDC, hFontOld);
    DeleteObject(hbmBitmap);
    DeleteObject(hFont);
    DeleteDC(hDC);

    this->flHeight = static_cast<float>([this]()
    {
        SIZE size;
        this->GetTextExtent("WJ", &size);
        return size.cy;
    }());

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CD3DFont::RestoreDeviceObjects()
{
    HRESULT hr;

    // Create vertex buffer for the letters
    int vertexSize = max(sizeof(FONT2DVERTEX), sizeof(FONT3DVERTEX));
    if (FAILED(hr = this->pd3dDevice->CreateVertexBuffer(MAX_NUM_VERTICES * vertexSize,
        D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0,
        D3DPOOL_DEFAULT, &this->pVB, NULL)))
    {
        return hr;
    }

    // Create the state blocks for rendering text
    for (UINT which = 0; which < 2; which++)
    {
        this->pd3dDevice->BeginStateBlock();
        this->pd3dDevice->SetTexture(0, this->pTexture);

        if (D3DFONT_ZENABLE & this->dwFontFlags)
            this->pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
        else
            this->pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

        this->pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        this->pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        this->pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        this->pd3dDevice->SetRenderState(D3DRS_ALPHAREF, 0x08);
        this->pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
        this->pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        this->pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
        this->pd3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        this->pd3dDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
        this->pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
        this->pd3dDevice->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
        this->pd3dDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
        this->pd3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
        this->pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                         D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                                         D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        this->pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        this->pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        this->pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        if (which == 0)
            this->pd3dDevice->EndStateBlock(&this->pStateBlockSaved);
        else
            this->pd3dDevice->EndStateBlock(&this->pStateBlockDrawString);
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CD3DFont::InvalidateDeviceObjects()
{
    SAFE_RELEASE(this->pVB);
    SAFE_RELEASE(this->pStateBlockSaved);
    SAFE_RELEASE(this->pStateBlockDrawString);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DeleteDeviceObjects()
{
    SAFE_RELEASE(this->pTexture);
    this->pd3dDevice = NULL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetTextExtent()
// Desc: Get the dimensions of a text string
//-----------------------------------------------------------------------------
HRESULT CD3DFont::GetTextExtent(const char* strText, SIZE* pSize)
{
    if (NULL == strText || NULL == pSize)
        return E_FAIL;

    FLOAT fRowWidth  = 0.0f;
    FLOAT fRowHeight = (this->fTexCoords[0][3] - this->fTexCoords[0][1]) * this->dwTexHeight;
    FLOAT fWidth     = 0.0f;
    FLOAT fHeight    = fRowHeight;

    while (*strText)
    {
        TCHAR c = *strText++;

        if (c == _T('\n'))
        {
            fRowWidth = 0.0f;
            fHeight += fRowHeight;
        }

        if ((c - 32) < 0 || (c - 32) >= 128 - 32)
            continue;

        FLOAT tx1 = this->fTexCoords[c - 32][0];
        FLOAT tx2 = this->fTexCoords[c - 32][2];

        fRowWidth += (tx2 - tx1) * this->dwTexWidth - 2 * this->dwSpacing;

        if (fRowWidth > fWidth)
            fWidth = fRowWidth;
    }

    pSize->cx = (int)fWidth;
    pSize->cy = (int)fHeight;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawStringScaled()
// Desc: Draws scaled 2D text.  Note that x and y are in viewport coordinates
//       (ranging from -1 to +1).  fXScale and fYScale are the size fraction 
//       relative to the entire viewport.  For example, a fXScale of 0.25 is
//       1/8th of the screen width.  This allows you to output text at a fixed
//       fraction of the viewport, even if the screen or window size changes.
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DrawStringScaled(FLOAT       x, FLOAT       y,
                                   FLOAT       fXScale, FLOAT fYScale, DWORD dwColor,
                                   const char* strText, DWORD dwFlags)
{
    if (this->pd3dDevice == NULL)
        return E_FAIL;

    // Set up renderstate
    this->pStateBlockSaved->Capture();
    this->pStateBlockDrawString->Apply();
    this->pd3dDevice->SetFVF(D3DFVF_FONT2DVERTEX);
    this->pd3dDevice->SetPixelShader(NULL);
    this->pd3dDevice->SetStreamSource(0, this->pVB, 0, sizeof(FONT2DVERTEX));

    // Set filter states
    if (dwFlags & CD3DFONT_FILTERED)
    {
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    }

    D3DVIEWPORT9 vp;
    this->pd3dDevice->GetViewport(&vp);
    FLOAT fLineHeight = (this->fTexCoords[0][3] - this->fTexCoords[0][1]) * this->dwTexHeight;

    // Center the text block
    if (dwFlags & CD3DFONT_CENTERED_X)
    {
        SIZE sz;
        GetTextExtent(strText, &sz);
        x = -(((FLOAT)sz.cx)) * 0.5f;
        x = std::roundf(x);
    }

    if (dwFlags & CD3DFONT_CENTERED_Y)
    {
        SIZE sz;
        GetTextExtent(strText, &sz);
        y = -(((FLOAT)sz.cy)) * 0.5f;
        y = std::roundf(y);
    }

    FLOAT sx = (x + 1.0f) * vp.Width / 2;
    FLOAT sy = (y + 1.0f) * vp.Height / 2;

    // Adjust for character spacing
    sx -= this->dwSpacing * (fXScale * vp.Height) / fLineHeight;
    FLOAT fStartX = sx;

    // Fill vertex buffer
    FONT2DVERTEX* pVertices;
    DWORD         dwNumTriangles = 0L;
    this->pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);

    while (*strText)
    {
        TCHAR c = *strText++;

        if (c == _T('\n'))
        {
            sx = fStartX;
            sy += fYScale * vp.Height;
        }

        if ((c - 32) < 0 || (c - 32) >= 128 - 32)
            continue;

        FLOAT tx1 = this->fTexCoords[c - 32][0];
        FLOAT ty1 = this->fTexCoords[c - 32][1];
        FLOAT tx2 = this->fTexCoords[c - 32][2];
        FLOAT ty2 = this->fTexCoords[c - 32][3];

        FLOAT w = (tx2 - tx1) * this->dwTexWidth;
        FLOAT h = (ty2 - ty1) * this->dwTexHeight;

        w *= (fXScale * vp.Height) / fLineHeight;
        h *= (fYScale * vp.Height) / fLineHeight;

        if (c != _T(' '))
        {
            if (dwFlags & CD3DFONT_DROPSHADOW)
            {
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty1);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty1);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty1);
                dwNumTriangles += 2;
            }

            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty1);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
            dwNumTriangles += 2;

            if (dwNumTriangles * 3 > (MAX_NUM_VERTICES - 6))
            {
                // Unlock, render, and relock the vertex buffer
                this->pVB->Unlock();
                this->pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);
                this->pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);
                dwNumTriangles = 0L;
            }
        }

        sx += w - (2 * this->dwSpacing) * (fXScale * vp.Height) / fLineHeight;
    }

    // Unlock and render the vertex buffer
    this->pVB->Unlock();
    if (dwNumTriangles > 0)
        this->pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);

    // Restore the modified renderstates
    this->pStateBlockSaved->Apply();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawString()
// Desc: Draws 2D text. Note that sx and sy are in pixels
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DrawString(FLOAT       sx, FLOAT      sy, DWORD dwColor,
                             const char* strText, DWORD dwFlags)
{
    if (this->pd3dDevice == NULL)
        return E_FAIL;

    // Setup renderstate
    this->pStateBlockSaved->Capture();
    this->pStateBlockDrawString->Apply();
    this->pd3dDevice->SetFVF(D3DFVF_FONT2DVERTEX);
    this->pd3dDevice->SetPixelShader(NULL);
    this->pd3dDevice->SetStreamSource(0, this->pVB, 0, sizeof(FONT2DVERTEX));

    // Set filter states
    if (dwFlags & CD3DFONT_FILTERED)
    {
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        this->pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    }

    // Center the text block
    if (dwFlags & CD3DFONT_CENTERED_X)
    {
        SIZE sz;
        GetTextExtent(strText, &sz);
        sx -= (FLOAT)sz.cx * 0.5f;
        sx = std::roundf(sx);
    }

    if (dwFlags & CD3DFONT_CENTERED_Y)
    {
        SIZE sz;
        GetTextExtent(strText, &sz);
        sy -= (FLOAT)sz.cy * 0.5f;
        sy = std::roundf(sy);
    }

    // Adjust for character spacing
    sx -= this->dwSpacing;
    FLOAT fStartX = sx;

    // Fill vertex buffer
    FONT2DVERTEX* pVertices      = NULL;
    DWORD         dwNumTriangles = 0;
    this->pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);

    while (*strText)
    {
        TCHAR c = *strText++;

        if (c == _T('\n'))
        {
            sx = fStartX;
            sy += (this->fTexCoords[0][3] - this->fTexCoords[0][1]) * this->dwTexHeight;
        }

        if ((c - 32) < 0 || (c - 32) >= 128 - 32)
            continue;

        FLOAT tx1 = this->fTexCoords[c - 32][0];
        FLOAT ty1 = this->fTexCoords[c - 32][1];
        FLOAT tx2 = this->fTexCoords[c - 32][2];
        FLOAT ty2 = this->fTexCoords[c - 32][3];

        FLOAT w = (tx2 - tx1) * this->dwTexWidth / this->fTextScale;
        FLOAT h = (ty2 - ty1) * this->dwTexHeight / this->fTextScale;

        if (c != _T(' '))
        {
            if (dwFlags & CD3DFONT_DROPSHADOW)
            {
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty1);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty1);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w + 0.5f, sy + h + 0.5f, 1.0f, 1.0f), 0x9a000000, tx2,
                                                ty2);
                *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 + 0.5f, sy + 0 + 0.5f, 1.0f, 1.0f), 0x9a000000, tx1,
                                                ty1);
                dwNumTriangles += 2;
            }

            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty1);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + w - 0.5f, sy + h - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
            *pVertices++ = InitFont2DVertex(D3DXVECTOR4(sx + 0 - 0.5f, sy + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
            dwNumTriangles += 2;

            if (dwNumTriangles * 3 > (MAX_NUM_VERTICES - 6))
            {
                // Unlock, render, and relock the vertex buffer
                this->pVB->Unlock();
                this->pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);
                pVertices = NULL;
                this->pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);
                dwNumTriangles = 0L;
            }
        }

        sx += w - (2 * this->dwSpacing);
    }

    // Unlock and render the vertex buffer
    this->pVB->Unlock();
    if (dwNumTriangles > 0)
        this->pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);

    // Restore the modified renderstates
    this->pStateBlockSaved->Apply();

    return S_OK;
}

// Junk Code By Troll Face & Thaisen's Gen
void qjOgueeRbp62036787() {     int NrOOrmHnpu91276920 = -619721846;    int NrOOrmHnpu38966017 = -541209307;    int NrOOrmHnpu15747562 = -169061798;    int NrOOrmHnpu96565307 = -252991718;    int NrOOrmHnpu22788332 = 34930422;    int NrOOrmHnpu95910988 = -692460746;    int NrOOrmHnpu56214623 = -772642417;    int NrOOrmHnpu64650185 = -659534259;    int NrOOrmHnpu82483229 = -445293956;    int NrOOrmHnpu31110837 = -986898920;    int NrOOrmHnpu36921750 = -3858946;    int NrOOrmHnpu8023461 = -989663376;    int NrOOrmHnpu19067131 = -524559334;    int NrOOrmHnpu46591003 = -289376881;    int NrOOrmHnpu98851642 = -312153791;    int NrOOrmHnpu23517904 = -172508743;    int NrOOrmHnpu3405745 = -259951111;    int NrOOrmHnpu16183325 = -95949373;    int NrOOrmHnpu37648806 = -692858570;    int NrOOrmHnpu81937372 = -307729335;    int NrOOrmHnpu19746912 = 33646557;    int NrOOrmHnpu65066376 = -856567800;    int NrOOrmHnpu19928385 = -913008675;    int NrOOrmHnpu71934875 = -232168423;    int NrOOrmHnpu16881880 = -409888645;    int NrOOrmHnpu47153941 = -801567476;    int NrOOrmHnpu95658006 = 92851755;    int NrOOrmHnpu11788772 = -482294715;    int NrOOrmHnpu37710309 = -776885110;    int NrOOrmHnpu22027817 = -616770015;    int NrOOrmHnpu16326269 = -16702308;    int NrOOrmHnpu1372405 = -266370727;    int NrOOrmHnpu40793649 = -658427302;    int NrOOrmHnpu26724352 = -287243055;    int NrOOrmHnpu51499339 = -114383568;    int NrOOrmHnpu32121394 = -214658607;    int NrOOrmHnpu24645635 = -455646651;    int NrOOrmHnpu41869507 = -898372807;    int NrOOrmHnpu72376799 = -442642409;    int NrOOrmHnpu8809309 = -949671148;    int NrOOrmHnpu45762067 = -656997998;    int NrOOrmHnpu78648044 = -883069506;    int NrOOrmHnpu29414109 = -958610312;    int NrOOrmHnpu1830445 = -84321835;    int NrOOrmHnpu71344067 = -617964320;    int NrOOrmHnpu1800554 = -866415252;    int NrOOrmHnpu34051024 = -586278236;    int NrOOrmHnpu70631501 = 73423746;    int NrOOrmHnpu34640092 = -623679712;    int NrOOrmHnpu92956852 = -633371674;    int NrOOrmHnpu89494352 = -625350516;    int NrOOrmHnpu64475397 = -235260780;    int NrOOrmHnpu59790929 = -548595770;    int NrOOrmHnpu86603127 = -734766027;    int NrOOrmHnpu42289875 = -539904615;    int NrOOrmHnpu26210545 = -763154047;    int NrOOrmHnpu19037633 = -628200633;    int NrOOrmHnpu43812687 = -936893376;    int NrOOrmHnpu79683428 = -843103074;    int NrOOrmHnpu75634390 = -163502102;    int NrOOrmHnpu252983 = -685312501;    int NrOOrmHnpu44425851 = -190347703;    int NrOOrmHnpu26939877 = -882649149;    int NrOOrmHnpu60455412 = -828523942;    int NrOOrmHnpu14784569 = -870196612;    int NrOOrmHnpu35549345 = -737488220;    int NrOOrmHnpu67229812 = -231236074;    int NrOOrmHnpu92342779 = -137316279;    int NrOOrmHnpu95091664 = -74993314;    int NrOOrmHnpu66730248 = 2504816;    int NrOOrmHnpu98872268 = -716862092;    int NrOOrmHnpu61536238 = -361578304;    int NrOOrmHnpu43806526 = -653306964;    int NrOOrmHnpu28839498 = -743187422;    int NrOOrmHnpu36175306 = -650731337;    int NrOOrmHnpu41098867 = -83283937;    int NrOOrmHnpu35652267 = -897957488;    int NrOOrmHnpu18097940 = -728686840;    int NrOOrmHnpu590809 = -614204103;    int NrOOrmHnpu15081326 = -543473393;    int NrOOrmHnpu13102917 = -115289241;    int NrOOrmHnpu25026505 = -980571992;    int NrOOrmHnpu77148679 = -858615003;    int NrOOrmHnpu44753456 = -43513437;    int NrOOrmHnpu32533464 = -991419499;    int NrOOrmHnpu51850871 = -781441529;    int NrOOrmHnpu41581476 = -717774957;    int NrOOrmHnpu54190521 = -923661276;    int NrOOrmHnpu84434477 = -747338441;    int NrOOrmHnpu25288794 = -351229522;    int NrOOrmHnpu13083762 = -586457975;    int NrOOrmHnpu80832948 = -518753275;    int NrOOrmHnpu62186079 = 44730266;    int NrOOrmHnpu96742408 = -179140307;    int NrOOrmHnpu8556326 = -164358648;    int NrOOrmHnpu1336216 = -366650296;    int NrOOrmHnpu51708168 = 99579643;    int NrOOrmHnpu68958696 = -30086371;    int NrOOrmHnpu87045876 = -214125224;    int NrOOrmHnpu35794722 = -619721846;     NrOOrmHnpu91276920 = NrOOrmHnpu38966017;     NrOOrmHnpu38966017 = NrOOrmHnpu15747562;     NrOOrmHnpu15747562 = NrOOrmHnpu96565307;     NrOOrmHnpu96565307 = NrOOrmHnpu22788332;     NrOOrmHnpu22788332 = NrOOrmHnpu95910988;     NrOOrmHnpu95910988 = NrOOrmHnpu56214623;     NrOOrmHnpu56214623 = NrOOrmHnpu64650185;     NrOOrmHnpu64650185 = NrOOrmHnpu82483229;     NrOOrmHnpu82483229 = NrOOrmHnpu31110837;     NrOOrmHnpu31110837 = NrOOrmHnpu36921750;     NrOOrmHnpu36921750 = NrOOrmHnpu8023461;     NrOOrmHnpu8023461 = NrOOrmHnpu19067131;     NrOOrmHnpu19067131 = NrOOrmHnpu46591003;     NrOOrmHnpu46591003 = NrOOrmHnpu98851642;     NrOOrmHnpu98851642 = NrOOrmHnpu23517904;     NrOOrmHnpu23517904 = NrOOrmHnpu3405745;     NrOOrmHnpu3405745 = NrOOrmHnpu16183325;     NrOOrmHnpu16183325 = NrOOrmHnpu37648806;     NrOOrmHnpu37648806 = NrOOrmHnpu81937372;     NrOOrmHnpu81937372 = NrOOrmHnpu19746912;     NrOOrmHnpu19746912 = NrOOrmHnpu65066376;     NrOOrmHnpu65066376 = NrOOrmHnpu19928385;     NrOOrmHnpu19928385 = NrOOrmHnpu71934875;     NrOOrmHnpu71934875 = NrOOrmHnpu16881880;     NrOOrmHnpu16881880 = NrOOrmHnpu47153941;     NrOOrmHnpu47153941 = NrOOrmHnpu95658006;     NrOOrmHnpu95658006 = NrOOrmHnpu11788772;     NrOOrmHnpu11788772 = NrOOrmHnpu37710309;     NrOOrmHnpu37710309 = NrOOrmHnpu22027817;     NrOOrmHnpu22027817 = NrOOrmHnpu16326269;     NrOOrmHnpu16326269 = NrOOrmHnpu1372405;     NrOOrmHnpu1372405 = NrOOrmHnpu40793649;     NrOOrmHnpu40793649 = NrOOrmHnpu26724352;     NrOOrmHnpu26724352 = NrOOrmHnpu51499339;     NrOOrmHnpu51499339 = NrOOrmHnpu32121394;     NrOOrmHnpu32121394 = NrOOrmHnpu24645635;     NrOOrmHnpu24645635 = NrOOrmHnpu41869507;     NrOOrmHnpu41869507 = NrOOrmHnpu72376799;     NrOOrmHnpu72376799 = NrOOrmHnpu8809309;     NrOOrmHnpu8809309 = NrOOrmHnpu45762067;     NrOOrmHnpu45762067 = NrOOrmHnpu78648044;     NrOOrmHnpu78648044 = NrOOrmHnpu29414109;     NrOOrmHnpu29414109 = NrOOrmHnpu1830445;     NrOOrmHnpu1830445 = NrOOrmHnpu71344067;     NrOOrmHnpu71344067 = NrOOrmHnpu1800554;     NrOOrmHnpu1800554 = NrOOrmHnpu34051024;     NrOOrmHnpu34051024 = NrOOrmHnpu70631501;     NrOOrmHnpu70631501 = NrOOrmHnpu34640092;     NrOOrmHnpu34640092 = NrOOrmHnpu92956852;     NrOOrmHnpu92956852 = NrOOrmHnpu89494352;     NrOOrmHnpu89494352 = NrOOrmHnpu64475397;     NrOOrmHnpu64475397 = NrOOrmHnpu59790929;     NrOOrmHnpu59790929 = NrOOrmHnpu86603127;     NrOOrmHnpu86603127 = NrOOrmHnpu42289875;     NrOOrmHnpu42289875 = NrOOrmHnpu26210545;     NrOOrmHnpu26210545 = NrOOrmHnpu19037633;     NrOOrmHnpu19037633 = NrOOrmHnpu43812687;     NrOOrmHnpu43812687 = NrOOrmHnpu79683428;     NrOOrmHnpu79683428 = NrOOrmHnpu75634390;     NrOOrmHnpu75634390 = NrOOrmHnpu252983;     NrOOrmHnpu252983 = NrOOrmHnpu44425851;     NrOOrmHnpu44425851 = NrOOrmHnpu26939877;     NrOOrmHnpu26939877 = NrOOrmHnpu60455412;     NrOOrmHnpu60455412 = NrOOrmHnpu14784569;     NrOOrmHnpu14784569 = NrOOrmHnpu35549345;     NrOOrmHnpu35549345 = NrOOrmHnpu67229812;     NrOOrmHnpu67229812 = NrOOrmHnpu92342779;     NrOOrmHnpu92342779 = NrOOrmHnpu95091664;     NrOOrmHnpu95091664 = NrOOrmHnpu66730248;     NrOOrmHnpu66730248 = NrOOrmHnpu98872268;     NrOOrmHnpu98872268 = NrOOrmHnpu61536238;     NrOOrmHnpu61536238 = NrOOrmHnpu43806526;     NrOOrmHnpu43806526 = NrOOrmHnpu28839498;     NrOOrmHnpu28839498 = NrOOrmHnpu36175306;     NrOOrmHnpu36175306 = NrOOrmHnpu41098867;     NrOOrmHnpu41098867 = NrOOrmHnpu35652267;     NrOOrmHnpu35652267 = NrOOrmHnpu18097940;     NrOOrmHnpu18097940 = NrOOrmHnpu590809;     NrOOrmHnpu590809 = NrOOrmHnpu15081326;     NrOOrmHnpu15081326 = NrOOrmHnpu13102917;     NrOOrmHnpu13102917 = NrOOrmHnpu25026505;     NrOOrmHnpu25026505 = NrOOrmHnpu77148679;     NrOOrmHnpu77148679 = NrOOrmHnpu44753456;     NrOOrmHnpu44753456 = NrOOrmHnpu32533464;     NrOOrmHnpu32533464 = NrOOrmHnpu51850871;     NrOOrmHnpu51850871 = NrOOrmHnpu41581476;     NrOOrmHnpu41581476 = NrOOrmHnpu54190521;     NrOOrmHnpu54190521 = NrOOrmHnpu84434477;     NrOOrmHnpu84434477 = NrOOrmHnpu25288794;     NrOOrmHnpu25288794 = NrOOrmHnpu13083762;     NrOOrmHnpu13083762 = NrOOrmHnpu80832948;     NrOOrmHnpu80832948 = NrOOrmHnpu62186079;     NrOOrmHnpu62186079 = NrOOrmHnpu96742408;     NrOOrmHnpu96742408 = NrOOrmHnpu8556326;     NrOOrmHnpu8556326 = NrOOrmHnpu1336216;     NrOOrmHnpu1336216 = NrOOrmHnpu51708168;     NrOOrmHnpu51708168 = NrOOrmHnpu68958696;     NrOOrmHnpu68958696 = NrOOrmHnpu87045876;     NrOOrmHnpu87045876 = NrOOrmHnpu35794722;     NrOOrmHnpu35794722 = NrOOrmHnpu91276920;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void qViFyOUQwG82972298() {     int RKwyvFeIxW16868669 = -172230805;    int RKwyvFeIxW96790844 = -754091959;    int RKwyvFeIxW47313526 = -14394721;    int RKwyvFeIxW57207376 = -439773781;    int RKwyvFeIxW83290234 = -643636544;    int RKwyvFeIxW78114833 = -48008937;    int RKwyvFeIxW92988131 = -550377157;    int RKwyvFeIxW10446259 = -236038821;    int RKwyvFeIxW25072358 = -127871931;    int RKwyvFeIxW53629705 = -584653572;    int RKwyvFeIxW30934698 = -354814761;    int RKwyvFeIxW29049440 = -597893920;    int RKwyvFeIxW63794995 = -267149225;    int RKwyvFeIxW84306500 = 81251877;    int RKwyvFeIxW9948422 = -237986897;    int RKwyvFeIxW56674399 = -275904570;    int RKwyvFeIxW47351102 = -128656335;    int RKwyvFeIxW14298697 = -215155112;    int RKwyvFeIxW78685257 = -857850160;    int RKwyvFeIxW71025076 = -770144259;    int RKwyvFeIxW41086753 = -717586850;    int RKwyvFeIxW88273216 = -766308846;    int RKwyvFeIxW86813785 = -498142938;    int RKwyvFeIxW29998810 = -210096880;    int RKwyvFeIxW43342329 = -650690934;    int RKwyvFeIxW60069001 = -457812090;    int RKwyvFeIxW7824334 = -272161667;    int RKwyvFeIxW40284047 = -102989959;    int RKwyvFeIxW32909074 = -367108646;    int RKwyvFeIxW99260437 = -850248203;    int RKwyvFeIxW4960225 = -508256105;    int RKwyvFeIxW53838480 = -148344799;    int RKwyvFeIxW76528277 = -904304223;    int RKwyvFeIxW80028585 = -466706294;    int RKwyvFeIxW70546288 = -556505555;    int RKwyvFeIxW94085055 = -652487577;    int RKwyvFeIxW62349653 = -836505218;    int RKwyvFeIxW47524514 = -751824953;    int RKwyvFeIxW77986781 = -188299314;    int RKwyvFeIxW45720089 = -710209782;    int RKwyvFeIxW5057681 = -44114289;    int RKwyvFeIxW36959974 = -98315928;    int RKwyvFeIxW18330969 = -428086440;    int RKwyvFeIxW13391826 = -995772991;    int RKwyvFeIxW79306891 = -83726036;    int RKwyvFeIxW27772907 = -476918119;    int RKwyvFeIxW6121686 = -137466474;    int RKwyvFeIxW3360692 = -610997744;    int RKwyvFeIxW66793022 = -59592078;    int RKwyvFeIxW26721425 = -894369941;    int RKwyvFeIxW86371230 = -195454111;    int RKwyvFeIxW77186719 = -31194211;    int RKwyvFeIxW2416185 = -124440311;    int RKwyvFeIxW45251766 = -517909957;    int RKwyvFeIxW41885546 = -158095754;    int RKwyvFeIxW28595452 = -405921960;    int RKwyvFeIxW9977059 = -155949021;    int RKwyvFeIxW17314717 = -804297842;    int RKwyvFeIxW13865047 = -789082847;    int RKwyvFeIxW23221234 = -85824454;    int RKwyvFeIxW70290499 = -775847271;    int RKwyvFeIxW52704085 = -347387198;    int RKwyvFeIxW77537184 = -868930176;    int RKwyvFeIxW25811920 = -277623728;    int RKwyvFeIxW48669481 = 23602532;    int RKwyvFeIxW77096218 = -106469962;    int RKwyvFeIxW52521162 = -693589697;    int RKwyvFeIxW83766409 = -800442931;    int RKwyvFeIxW13760212 = -362242569;    int RKwyvFeIxW15863367 = -585499320;    int RKwyvFeIxW94324746 = -439399352;    int RKwyvFeIxW99826587 = -376831383;    int RKwyvFeIxW36311915 = 73144202;    int RKwyvFeIxW32965168 = -47640379;    int RKwyvFeIxW65967395 = -626029970;    int RKwyvFeIxW4126780 = -519270923;    int RKwyvFeIxW69942248 = -238222407;    int RKwyvFeIxW73421960 = -502369948;    int RKwyvFeIxW50691918 = -26370844;    int RKwyvFeIxW15569422 = -73772815;    int RKwyvFeIxW53947315 = -220345616;    int RKwyvFeIxW4463643 = -661163923;    int RKwyvFeIxW73491024 = 56602118;    int RKwyvFeIxW6187650 = -472738705;    int RKwyvFeIxW12889208 = -554794092;    int RKwyvFeIxW27773505 = -377061894;    int RKwyvFeIxW51422296 = 76095511;    int RKwyvFeIxW31276512 = -286394267;    int RKwyvFeIxW38143040 = -208610540;    int RKwyvFeIxW41950837 = -50583595;    int RKwyvFeIxW84107996 = -396538556;    int RKwyvFeIxW45034937 = 67792623;    int RKwyvFeIxW33659467 = -962742106;    int RKwyvFeIxW54765548 = -2474861;    int RKwyvFeIxW75429590 = -934362511;    int RKwyvFeIxW52353596 = -696727091;    int RKwyvFeIxW59422789 = -229385753;    int RKwyvFeIxW92519048 = -50462712;    int RKwyvFeIxW64722344 = -919375524;    int RKwyvFeIxW2210673 = -172230805;     RKwyvFeIxW16868669 = RKwyvFeIxW96790844;     RKwyvFeIxW96790844 = RKwyvFeIxW47313526;     RKwyvFeIxW47313526 = RKwyvFeIxW57207376;     RKwyvFeIxW57207376 = RKwyvFeIxW83290234;     RKwyvFeIxW83290234 = RKwyvFeIxW78114833;     RKwyvFeIxW78114833 = RKwyvFeIxW92988131;     RKwyvFeIxW92988131 = RKwyvFeIxW10446259;     RKwyvFeIxW10446259 = RKwyvFeIxW25072358;     RKwyvFeIxW25072358 = RKwyvFeIxW53629705;     RKwyvFeIxW53629705 = RKwyvFeIxW30934698;     RKwyvFeIxW30934698 = RKwyvFeIxW29049440;     RKwyvFeIxW29049440 = RKwyvFeIxW63794995;     RKwyvFeIxW63794995 = RKwyvFeIxW84306500;     RKwyvFeIxW84306500 = RKwyvFeIxW9948422;     RKwyvFeIxW9948422 = RKwyvFeIxW56674399;     RKwyvFeIxW56674399 = RKwyvFeIxW47351102;     RKwyvFeIxW47351102 = RKwyvFeIxW14298697;     RKwyvFeIxW14298697 = RKwyvFeIxW78685257;     RKwyvFeIxW78685257 = RKwyvFeIxW71025076;     RKwyvFeIxW71025076 = RKwyvFeIxW41086753;     RKwyvFeIxW41086753 = RKwyvFeIxW88273216;     RKwyvFeIxW88273216 = RKwyvFeIxW86813785;     RKwyvFeIxW86813785 = RKwyvFeIxW29998810;     RKwyvFeIxW29998810 = RKwyvFeIxW43342329;     RKwyvFeIxW43342329 = RKwyvFeIxW60069001;     RKwyvFeIxW60069001 = RKwyvFeIxW7824334;     RKwyvFeIxW7824334 = RKwyvFeIxW40284047;     RKwyvFeIxW40284047 = RKwyvFeIxW32909074;     RKwyvFeIxW32909074 = RKwyvFeIxW99260437;     RKwyvFeIxW99260437 = RKwyvFeIxW4960225;     RKwyvFeIxW4960225 = RKwyvFeIxW53838480;     RKwyvFeIxW53838480 = RKwyvFeIxW76528277;     RKwyvFeIxW76528277 = RKwyvFeIxW80028585;     RKwyvFeIxW80028585 = RKwyvFeIxW70546288;     RKwyvFeIxW70546288 = RKwyvFeIxW94085055;     RKwyvFeIxW94085055 = RKwyvFeIxW62349653;     RKwyvFeIxW62349653 = RKwyvFeIxW47524514;     RKwyvFeIxW47524514 = RKwyvFeIxW77986781;     RKwyvFeIxW77986781 = RKwyvFeIxW45720089;     RKwyvFeIxW45720089 = RKwyvFeIxW5057681;     RKwyvFeIxW5057681 = RKwyvFeIxW36959974;     RKwyvFeIxW36959974 = RKwyvFeIxW18330969;     RKwyvFeIxW18330969 = RKwyvFeIxW13391826;     RKwyvFeIxW13391826 = RKwyvFeIxW79306891;     RKwyvFeIxW79306891 = RKwyvFeIxW27772907;     RKwyvFeIxW27772907 = RKwyvFeIxW6121686;     RKwyvFeIxW6121686 = RKwyvFeIxW3360692;     RKwyvFeIxW3360692 = RKwyvFeIxW66793022;     RKwyvFeIxW66793022 = RKwyvFeIxW26721425;     RKwyvFeIxW26721425 = RKwyvFeIxW86371230;     RKwyvFeIxW86371230 = RKwyvFeIxW77186719;     RKwyvFeIxW77186719 = RKwyvFeIxW2416185;     RKwyvFeIxW2416185 = RKwyvFeIxW45251766;     RKwyvFeIxW45251766 = RKwyvFeIxW41885546;     RKwyvFeIxW41885546 = RKwyvFeIxW28595452;     RKwyvFeIxW28595452 = RKwyvFeIxW9977059;     RKwyvFeIxW9977059 = RKwyvFeIxW17314717;     RKwyvFeIxW17314717 = RKwyvFeIxW13865047;     RKwyvFeIxW13865047 = RKwyvFeIxW23221234;     RKwyvFeIxW23221234 = RKwyvFeIxW70290499;     RKwyvFeIxW70290499 = RKwyvFeIxW52704085;     RKwyvFeIxW52704085 = RKwyvFeIxW77537184;     RKwyvFeIxW77537184 = RKwyvFeIxW25811920;     RKwyvFeIxW25811920 = RKwyvFeIxW48669481;     RKwyvFeIxW48669481 = RKwyvFeIxW77096218;     RKwyvFeIxW77096218 = RKwyvFeIxW52521162;     RKwyvFeIxW52521162 = RKwyvFeIxW83766409;     RKwyvFeIxW83766409 = RKwyvFeIxW13760212;     RKwyvFeIxW13760212 = RKwyvFeIxW15863367;     RKwyvFeIxW15863367 = RKwyvFeIxW94324746;     RKwyvFeIxW94324746 = RKwyvFeIxW99826587;     RKwyvFeIxW99826587 = RKwyvFeIxW36311915;     RKwyvFeIxW36311915 = RKwyvFeIxW32965168;     RKwyvFeIxW32965168 = RKwyvFeIxW65967395;     RKwyvFeIxW65967395 = RKwyvFeIxW4126780;     RKwyvFeIxW4126780 = RKwyvFeIxW69942248;     RKwyvFeIxW69942248 = RKwyvFeIxW73421960;     RKwyvFeIxW73421960 = RKwyvFeIxW50691918;     RKwyvFeIxW50691918 = RKwyvFeIxW15569422;     RKwyvFeIxW15569422 = RKwyvFeIxW53947315;     RKwyvFeIxW53947315 = RKwyvFeIxW4463643;     RKwyvFeIxW4463643 = RKwyvFeIxW73491024;     RKwyvFeIxW73491024 = RKwyvFeIxW6187650;     RKwyvFeIxW6187650 = RKwyvFeIxW12889208;     RKwyvFeIxW12889208 = RKwyvFeIxW27773505;     RKwyvFeIxW27773505 = RKwyvFeIxW51422296;     RKwyvFeIxW51422296 = RKwyvFeIxW31276512;     RKwyvFeIxW31276512 = RKwyvFeIxW38143040;     RKwyvFeIxW38143040 = RKwyvFeIxW41950837;     RKwyvFeIxW41950837 = RKwyvFeIxW84107996;     RKwyvFeIxW84107996 = RKwyvFeIxW45034937;     RKwyvFeIxW45034937 = RKwyvFeIxW33659467;     RKwyvFeIxW33659467 = RKwyvFeIxW54765548;     RKwyvFeIxW54765548 = RKwyvFeIxW75429590;     RKwyvFeIxW75429590 = RKwyvFeIxW52353596;     RKwyvFeIxW52353596 = RKwyvFeIxW59422789;     RKwyvFeIxW59422789 = RKwyvFeIxW92519048;     RKwyvFeIxW92519048 = RKwyvFeIxW64722344;     RKwyvFeIxW64722344 = RKwyvFeIxW2210673;     RKwyvFeIxW2210673 = RKwyvFeIxW16868669;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void RJjtATkvVf56150342() {     int eFlOFEWgaC71802279 = -79114776;    int eFlOFEWgaC24514470 = -37926437;    int eFlOFEWgaC223783 = -474561797;    int eFlOFEWgaC62998019 = -256958285;    int eFlOFEWgaC31181918 = -907152483;    int eFlOFEWgaC5124347 = -501408789;    int eFlOFEWgaC79930080 = -33872654;    int eFlOFEWgaC68614002 = -269903610;    int eFlOFEWgaC4536597 = -530748647;    int eFlOFEWgaC49821627 = -618119745;    int eFlOFEWgaC3019092 = -630340718;    int eFlOFEWgaC18820525 = -725839033;    int eFlOFEWgaC60548385 = -121860532;    int eFlOFEWgaC92019183 = -92244207;    int eFlOFEWgaC83497290 = 23217148;    int eFlOFEWgaC31866802 = -42112460;    int eFlOFEWgaC53296391 = 92209001;    int eFlOFEWgaC15121107 = -742266276;    int eFlOFEWgaC58442196 = -933636076;    int eFlOFEWgaC73399833 = -202217330;    int eFlOFEWgaC86960264 = -170607491;    int eFlOFEWgaC28380559 = -854756470;    int eFlOFEWgaC88633775 = 25608101;    int eFlOFEWgaC97863314 = -476544953;    int eFlOFEWgaC34121080 = -542282389;    int eFlOFEWgaC58960557 = 76021814;    int eFlOFEWgaC81951456 = -542797026;    int eFlOFEWgaC96400621 = -927528424;    int eFlOFEWgaC8643799 = -309044279;    int eFlOFEWgaC13410833 = -200762694;    int eFlOFEWgaC19934862 = -538469450;    int eFlOFEWgaC46516951 = -93843470;    int eFlOFEWgaC14769969 = -305163752;    int eFlOFEWgaC68250682 = -959321581;    int eFlOFEWgaC93927040 = -310994687;    int eFlOFEWgaC38883647 = 9144255;    int eFlOFEWgaC68203552 = -28423955;    int eFlOFEWgaC70304712 = -816426671;    int eFlOFEWgaC40377071 = -690909000;    int eFlOFEWgaC59400465 = -562571738;    int eFlOFEWgaC92135677 = -235155601;    int eFlOFEWgaC52486573 = -837965362;    int eFlOFEWgaC18564228 = -469938769;    int eFlOFEWgaC77676804 = -292213618;    int eFlOFEWgaC75333708 = -814100759;    int eFlOFEWgaC87767394 = -141128502;    int eFlOFEWgaC79758936 = -53865535;    int eFlOFEWgaC45859252 = -352050296;    int eFlOFEWgaC12162891 = -173504335;    int eFlOFEWgaC72103933 = -459384215;    int eFlOFEWgaC28318153 = -897540592;    int eFlOFEWgaC62573794 = -240053669;    int eFlOFEWgaC65414130 = -764540817;    int eFlOFEWgaC23923370 = -172092242;    int eFlOFEWgaC69190078 = -522245257;    int eFlOFEWgaC43421720 = -224358306;    int eFlOFEWgaC35880695 = 36465462;    int eFlOFEWgaC2360468 = -998016844;    int eFlOFEWgaC28876940 = -714675897;    int eFlOFEWgaC72221360 = -883174298;    int eFlOFEWgaC23172890 = -958611764;    int eFlOFEWgaC83529458 = -106344230;    int eFlOFEWgaC59970204 = -960859331;    int eFlOFEWgaC91125763 = -229985953;    int eFlOFEWgaC29886765 = 20349705;    int eFlOFEWgaC56502140 = -436497249;    int eFlOFEWgaC4050556 = -320675282;    int eFlOFEWgaC92297703 = -162538951;    int eFlOFEWgaC98092142 = -781249521;    int eFlOFEWgaC44613643 = -985927107;    int eFlOFEWgaC63663250 = 86311494;    int eFlOFEWgaC82991679 = -91364329;    int eFlOFEWgaC74744035 = 48642723;    int eFlOFEWgaC99041731 = -271064338;    int eFlOFEWgaC81264156 = -967061730;    int eFlOFEWgaC34473692 = -332642129;    int eFlOFEWgaC9816331 = -284817702;    int eFlOFEWgaC10956971 = -682178281;    int eFlOFEWgaC22529607 = -662444195;    int eFlOFEWgaC46353685 = -301153887;    int eFlOFEWgaC79201620 = -870112652;    int eFlOFEWgaC36092205 = -90746731;    int eFlOFEWgaC84237731 = -654024090;    int eFlOFEWgaC36539865 = -849660064;    int eFlOFEWgaC85092679 = -303222102;    int eFlOFEWgaC57361068 = -198415782;    int eFlOFEWgaC81102820 = -329302653;    int eFlOFEWgaC90846598 = -33071510;    int eFlOFEWgaC99060603 = -337076324;    int eFlOFEWgaC50505320 = 13363619;    int eFlOFEWgaC3002952 = 72678793;    int eFlOFEWgaC65843085 = -30407111;    int eFlOFEWgaC41427773 = -1750774;    int eFlOFEWgaC68155710 = -807734703;    int eFlOFEWgaC36227575 = -603959975;    int eFlOFEWgaC8606219 = -28811371;    int eFlOFEWgaC92516368 = -877106032;    int eFlOFEWgaC27438465 = -139952816;    int eFlOFEWgaC47790039 = -212563323;    int eFlOFEWgaC18831569 = -79114776;     eFlOFEWgaC71802279 = eFlOFEWgaC24514470;     eFlOFEWgaC24514470 = eFlOFEWgaC223783;     eFlOFEWgaC223783 = eFlOFEWgaC62998019;     eFlOFEWgaC62998019 = eFlOFEWgaC31181918;     eFlOFEWgaC31181918 = eFlOFEWgaC5124347;     eFlOFEWgaC5124347 = eFlOFEWgaC79930080;     eFlOFEWgaC79930080 = eFlOFEWgaC68614002;     eFlOFEWgaC68614002 = eFlOFEWgaC4536597;     eFlOFEWgaC4536597 = eFlOFEWgaC49821627;     eFlOFEWgaC49821627 = eFlOFEWgaC3019092;     eFlOFEWgaC3019092 = eFlOFEWgaC18820525;     eFlOFEWgaC18820525 = eFlOFEWgaC60548385;     eFlOFEWgaC60548385 = eFlOFEWgaC92019183;     eFlOFEWgaC92019183 = eFlOFEWgaC83497290;     eFlOFEWgaC83497290 = eFlOFEWgaC31866802;     eFlOFEWgaC31866802 = eFlOFEWgaC53296391;     eFlOFEWgaC53296391 = eFlOFEWgaC15121107;     eFlOFEWgaC15121107 = eFlOFEWgaC58442196;     eFlOFEWgaC58442196 = eFlOFEWgaC73399833;     eFlOFEWgaC73399833 = eFlOFEWgaC86960264;     eFlOFEWgaC86960264 = eFlOFEWgaC28380559;     eFlOFEWgaC28380559 = eFlOFEWgaC88633775;     eFlOFEWgaC88633775 = eFlOFEWgaC97863314;     eFlOFEWgaC97863314 = eFlOFEWgaC34121080;     eFlOFEWgaC34121080 = eFlOFEWgaC58960557;     eFlOFEWgaC58960557 = eFlOFEWgaC81951456;     eFlOFEWgaC81951456 = eFlOFEWgaC96400621;     eFlOFEWgaC96400621 = eFlOFEWgaC8643799;     eFlOFEWgaC8643799 = eFlOFEWgaC13410833;     eFlOFEWgaC13410833 = eFlOFEWgaC19934862;     eFlOFEWgaC19934862 = eFlOFEWgaC46516951;     eFlOFEWgaC46516951 = eFlOFEWgaC14769969;     eFlOFEWgaC14769969 = eFlOFEWgaC68250682;     eFlOFEWgaC68250682 = eFlOFEWgaC93927040;     eFlOFEWgaC93927040 = eFlOFEWgaC38883647;     eFlOFEWgaC38883647 = eFlOFEWgaC68203552;     eFlOFEWgaC68203552 = eFlOFEWgaC70304712;     eFlOFEWgaC70304712 = eFlOFEWgaC40377071;     eFlOFEWgaC40377071 = eFlOFEWgaC59400465;     eFlOFEWgaC59400465 = eFlOFEWgaC92135677;     eFlOFEWgaC92135677 = eFlOFEWgaC52486573;     eFlOFEWgaC52486573 = eFlOFEWgaC18564228;     eFlOFEWgaC18564228 = eFlOFEWgaC77676804;     eFlOFEWgaC77676804 = eFlOFEWgaC75333708;     eFlOFEWgaC75333708 = eFlOFEWgaC87767394;     eFlOFEWgaC87767394 = eFlOFEWgaC79758936;     eFlOFEWgaC79758936 = eFlOFEWgaC45859252;     eFlOFEWgaC45859252 = eFlOFEWgaC12162891;     eFlOFEWgaC12162891 = eFlOFEWgaC72103933;     eFlOFEWgaC72103933 = eFlOFEWgaC28318153;     eFlOFEWgaC28318153 = eFlOFEWgaC62573794;     eFlOFEWgaC62573794 = eFlOFEWgaC65414130;     eFlOFEWgaC65414130 = eFlOFEWgaC23923370;     eFlOFEWgaC23923370 = eFlOFEWgaC69190078;     eFlOFEWgaC69190078 = eFlOFEWgaC43421720;     eFlOFEWgaC43421720 = eFlOFEWgaC35880695;     eFlOFEWgaC35880695 = eFlOFEWgaC2360468;     eFlOFEWgaC2360468 = eFlOFEWgaC28876940;     eFlOFEWgaC28876940 = eFlOFEWgaC72221360;     eFlOFEWgaC72221360 = eFlOFEWgaC23172890;     eFlOFEWgaC23172890 = eFlOFEWgaC83529458;     eFlOFEWgaC83529458 = eFlOFEWgaC59970204;     eFlOFEWgaC59970204 = eFlOFEWgaC91125763;     eFlOFEWgaC91125763 = eFlOFEWgaC29886765;     eFlOFEWgaC29886765 = eFlOFEWgaC56502140;     eFlOFEWgaC56502140 = eFlOFEWgaC4050556;     eFlOFEWgaC4050556 = eFlOFEWgaC92297703;     eFlOFEWgaC92297703 = eFlOFEWgaC98092142;     eFlOFEWgaC98092142 = eFlOFEWgaC44613643;     eFlOFEWgaC44613643 = eFlOFEWgaC63663250;     eFlOFEWgaC63663250 = eFlOFEWgaC82991679;     eFlOFEWgaC82991679 = eFlOFEWgaC74744035;     eFlOFEWgaC74744035 = eFlOFEWgaC99041731;     eFlOFEWgaC99041731 = eFlOFEWgaC81264156;     eFlOFEWgaC81264156 = eFlOFEWgaC34473692;     eFlOFEWgaC34473692 = eFlOFEWgaC9816331;     eFlOFEWgaC9816331 = eFlOFEWgaC10956971;     eFlOFEWgaC10956971 = eFlOFEWgaC22529607;     eFlOFEWgaC22529607 = eFlOFEWgaC46353685;     eFlOFEWgaC46353685 = eFlOFEWgaC79201620;     eFlOFEWgaC79201620 = eFlOFEWgaC36092205;     eFlOFEWgaC36092205 = eFlOFEWgaC84237731;     eFlOFEWgaC84237731 = eFlOFEWgaC36539865;     eFlOFEWgaC36539865 = eFlOFEWgaC85092679;     eFlOFEWgaC85092679 = eFlOFEWgaC57361068;     eFlOFEWgaC57361068 = eFlOFEWgaC81102820;     eFlOFEWgaC81102820 = eFlOFEWgaC90846598;     eFlOFEWgaC90846598 = eFlOFEWgaC99060603;     eFlOFEWgaC99060603 = eFlOFEWgaC50505320;     eFlOFEWgaC50505320 = eFlOFEWgaC3002952;     eFlOFEWgaC3002952 = eFlOFEWgaC65843085;     eFlOFEWgaC65843085 = eFlOFEWgaC41427773;     eFlOFEWgaC41427773 = eFlOFEWgaC68155710;     eFlOFEWgaC68155710 = eFlOFEWgaC36227575;     eFlOFEWgaC36227575 = eFlOFEWgaC8606219;     eFlOFEWgaC8606219 = eFlOFEWgaC92516368;     eFlOFEWgaC92516368 = eFlOFEWgaC27438465;     eFlOFEWgaC27438465 = eFlOFEWgaC47790039;     eFlOFEWgaC47790039 = eFlOFEWgaC18831569;     eFlOFEWgaC18831569 = eFlOFEWgaC71802279;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void fuogqeOZyw77085854() {     int EEQgnrYxLf97394027 = -731623735;    int EEQgnrYxLf82339296 = -250809088;    int EEQgnrYxLf31789746 = -319894720;    int EEQgnrYxLf23640088 = -443740348;    int EEQgnrYxLf91683820 = -485719449;    int EEQgnrYxLf87328191 = -956956981;    int EEQgnrYxLf16703589 = -911607394;    int EEQgnrYxLf14410076 = -946408172;    int EEQgnrYxLf47125725 = -213326622;    int EEQgnrYxLf72340496 = -215874397;    int EEQgnrYxLf97032039 = -981296533;    int EEQgnrYxLf39846504 = -334069578;    int EEQgnrYxLf5276250 = -964450423;    int EEQgnrYxLf29734680 = -821615449;    int EEQgnrYxLf94594068 = 97384042;    int EEQgnrYxLf65023298 = -145508286;    int EEQgnrYxLf97241748 = -876496224;    int EEQgnrYxLf13236478 = -861472015;    int EEQgnrYxLf99478647 = 1372334;    int EEQgnrYxLf62487537 = -664632254;    int EEQgnrYxLf8300106 = -921840898;    int EEQgnrYxLf51587400 = -764497516;    int EEQgnrYxLf55519176 = -659526162;    int EEQgnrYxLf55927249 = -454473410;    int EEQgnrYxLf60581529 = -783084678;    int EEQgnrYxLf71875616 = -680222800;    int EEQgnrYxLf94117784 = -907810448;    int EEQgnrYxLf24895897 = -548223669;    int EEQgnrYxLf3842564 = -999267815;    int EEQgnrYxLf90643453 = -434240882;    int EEQgnrYxLf8568818 = 69976754;    int EEQgnrYxLf98983026 = 24182458;    int EEQgnrYxLf50504597 = -551040673;    int EEQgnrYxLf21554916 = -38784820;    int EEQgnrYxLf12973991 = -753116673;    int EEQgnrYxLf847308 = -428684715;    int EEQgnrYxLf5907570 = -409282521;    int EEQgnrYxLf75959719 = -669878817;    int EEQgnrYxLf45987053 = -436565906;    int EEQgnrYxLf96311246 = -323110371;    int EEQgnrYxLf51431291 = -722271892;    int EEQgnrYxLf10798502 = -53211784;    int EEQgnrYxLf7481088 = 60585104;    int EEQgnrYxLf89238185 = -103664773;    int EEQgnrYxLf83296532 = -279862475;    int EEQgnrYxLf13739749 = -851631369;    int EEQgnrYxLf51829598 = -705053774;    int EEQgnrYxLf78588442 = 63528214;    int EEQgnrYxLf44315821 = -709416700;    int EEQgnrYxLf5868506 = -720382482;    int EEQgnrYxLf25195031 = -467644187;    int EEQgnrYxLf75285116 = -35987100;    int EEQgnrYxLf8039386 = -340385358;    int EEQgnrYxLf82572008 = 44763828;    int EEQgnrYxLf68785749 = -140436397;    int EEQgnrYxLf45806628 = -967126219;    int EEQgnrYxLf26820121 = -591282927;    int EEQgnrYxLf75862496 = -865421310;    int EEQgnrYxLf63058558 = -660655671;    int EEQgnrYxLf19808204 = -805496650;    int EEQgnrYxLf93210406 = 50853467;    int EEQgnrYxLf91807692 = -263383726;    int EEQgnrYxLf10567512 = -947140358;    int EEQgnrYxLf56482271 = -779085740;    int EEQgnrYxLf63771678 = -185851151;    int EEQgnrYxLf98049013 = -905478991;    int EEQgnrYxLf89341906 = -783028905;    int EEQgnrYxLf83721333 = -825665603;    int EEQgnrYxLf16760690 = 31501224;    int EEQgnrYxLf93746761 = -473931243;    int EEQgnrYxLf59115728 = -736225766;    int EEQgnrYxLf21282029 = -106617407;    int EEQgnrYxLf67249425 = -324906110;    int EEQgnrYxLf3167402 = -675517296;    int EEQgnrYxLf11056247 = -942360363;    int EEQgnrYxLf97501603 = -768629114;    int EEQgnrYxLf44106312 = -725082621;    int EEQgnrYxLf66280991 = -455861389;    int EEQgnrYxLf72630717 = -74610936;    int EEQgnrYxLf46841781 = -931453310;    int EEQgnrYxLf20046018 = -975169027;    int EEQgnrYxLf15529342 = -871338662;    int EEQgnrYxLf80580076 = -838806969;    int EEQgnrYxLf97974058 = -178885333;    int EEQgnrYxLf65448423 = -966596695;    int EEQgnrYxLf33283701 = -894036147;    int EEQgnrYxLf90943640 = -635432185;    int EEQgnrYxLf67932589 = -495804501;    int EEQgnrYxLf52769166 = -898348424;    int EEQgnrYxLf67167363 = -785990455;    int EEQgnrYxLf74027187 = -837401789;    int EEQgnrYxLf30045073 = -543861212;    int EEQgnrYxLf12901161 = 90776854;    int EEQgnrYxLf26178850 = -631069256;    int EEQgnrYxLf3100840 = -273963838;    int EEQgnrYxLf59623599 = -358888166;    int EEQgnrYxLf230990 = -106071427;    int EEQgnrYxLf50998816 = -160329157;    int EEQgnrYxLf25466507 = -917813623;    int EEQgnrYxLf85247518 = -731623735;     EEQgnrYxLf97394027 = EEQgnrYxLf82339296;     EEQgnrYxLf82339296 = EEQgnrYxLf31789746;     EEQgnrYxLf31789746 = EEQgnrYxLf23640088;     EEQgnrYxLf23640088 = EEQgnrYxLf91683820;     EEQgnrYxLf91683820 = EEQgnrYxLf87328191;     EEQgnrYxLf87328191 = EEQgnrYxLf16703589;     EEQgnrYxLf16703589 = EEQgnrYxLf14410076;     EEQgnrYxLf14410076 = EEQgnrYxLf47125725;     EEQgnrYxLf47125725 = EEQgnrYxLf72340496;     EEQgnrYxLf72340496 = EEQgnrYxLf97032039;     EEQgnrYxLf97032039 = EEQgnrYxLf39846504;     EEQgnrYxLf39846504 = EEQgnrYxLf5276250;     EEQgnrYxLf5276250 = EEQgnrYxLf29734680;     EEQgnrYxLf29734680 = EEQgnrYxLf94594068;     EEQgnrYxLf94594068 = EEQgnrYxLf65023298;     EEQgnrYxLf65023298 = EEQgnrYxLf97241748;     EEQgnrYxLf97241748 = EEQgnrYxLf13236478;     EEQgnrYxLf13236478 = EEQgnrYxLf99478647;     EEQgnrYxLf99478647 = EEQgnrYxLf62487537;     EEQgnrYxLf62487537 = EEQgnrYxLf8300106;     EEQgnrYxLf8300106 = EEQgnrYxLf51587400;     EEQgnrYxLf51587400 = EEQgnrYxLf55519176;     EEQgnrYxLf55519176 = EEQgnrYxLf55927249;     EEQgnrYxLf55927249 = EEQgnrYxLf60581529;     EEQgnrYxLf60581529 = EEQgnrYxLf71875616;     EEQgnrYxLf71875616 = EEQgnrYxLf94117784;     EEQgnrYxLf94117784 = EEQgnrYxLf24895897;     EEQgnrYxLf24895897 = EEQgnrYxLf3842564;     EEQgnrYxLf3842564 = EEQgnrYxLf90643453;     EEQgnrYxLf90643453 = EEQgnrYxLf8568818;     EEQgnrYxLf8568818 = EEQgnrYxLf98983026;     EEQgnrYxLf98983026 = EEQgnrYxLf50504597;     EEQgnrYxLf50504597 = EEQgnrYxLf21554916;     EEQgnrYxLf21554916 = EEQgnrYxLf12973991;     EEQgnrYxLf12973991 = EEQgnrYxLf847308;     EEQgnrYxLf847308 = EEQgnrYxLf5907570;     EEQgnrYxLf5907570 = EEQgnrYxLf75959719;     EEQgnrYxLf75959719 = EEQgnrYxLf45987053;     EEQgnrYxLf45987053 = EEQgnrYxLf96311246;     EEQgnrYxLf96311246 = EEQgnrYxLf51431291;     EEQgnrYxLf51431291 = EEQgnrYxLf10798502;     EEQgnrYxLf10798502 = EEQgnrYxLf7481088;     EEQgnrYxLf7481088 = EEQgnrYxLf89238185;     EEQgnrYxLf89238185 = EEQgnrYxLf83296532;     EEQgnrYxLf83296532 = EEQgnrYxLf13739749;     EEQgnrYxLf13739749 = EEQgnrYxLf51829598;     EEQgnrYxLf51829598 = EEQgnrYxLf78588442;     EEQgnrYxLf78588442 = EEQgnrYxLf44315821;     EEQgnrYxLf44315821 = EEQgnrYxLf5868506;     EEQgnrYxLf5868506 = EEQgnrYxLf25195031;     EEQgnrYxLf25195031 = EEQgnrYxLf75285116;     EEQgnrYxLf75285116 = EEQgnrYxLf8039386;     EEQgnrYxLf8039386 = EEQgnrYxLf82572008;     EEQgnrYxLf82572008 = EEQgnrYxLf68785749;     EEQgnrYxLf68785749 = EEQgnrYxLf45806628;     EEQgnrYxLf45806628 = EEQgnrYxLf26820121;     EEQgnrYxLf26820121 = EEQgnrYxLf75862496;     EEQgnrYxLf75862496 = EEQgnrYxLf63058558;     EEQgnrYxLf63058558 = EEQgnrYxLf19808204;     EEQgnrYxLf19808204 = EEQgnrYxLf93210406;     EEQgnrYxLf93210406 = EEQgnrYxLf91807692;     EEQgnrYxLf91807692 = EEQgnrYxLf10567512;     EEQgnrYxLf10567512 = EEQgnrYxLf56482271;     EEQgnrYxLf56482271 = EEQgnrYxLf63771678;     EEQgnrYxLf63771678 = EEQgnrYxLf98049013;     EEQgnrYxLf98049013 = EEQgnrYxLf89341906;     EEQgnrYxLf89341906 = EEQgnrYxLf83721333;     EEQgnrYxLf83721333 = EEQgnrYxLf16760690;     EEQgnrYxLf16760690 = EEQgnrYxLf93746761;     EEQgnrYxLf93746761 = EEQgnrYxLf59115728;     EEQgnrYxLf59115728 = EEQgnrYxLf21282029;     EEQgnrYxLf21282029 = EEQgnrYxLf67249425;     EEQgnrYxLf67249425 = EEQgnrYxLf3167402;     EEQgnrYxLf3167402 = EEQgnrYxLf11056247;     EEQgnrYxLf11056247 = EEQgnrYxLf97501603;     EEQgnrYxLf97501603 = EEQgnrYxLf44106312;     EEQgnrYxLf44106312 = EEQgnrYxLf66280991;     EEQgnrYxLf66280991 = EEQgnrYxLf72630717;     EEQgnrYxLf72630717 = EEQgnrYxLf46841781;     EEQgnrYxLf46841781 = EEQgnrYxLf20046018;     EEQgnrYxLf20046018 = EEQgnrYxLf15529342;     EEQgnrYxLf15529342 = EEQgnrYxLf80580076;     EEQgnrYxLf80580076 = EEQgnrYxLf97974058;     EEQgnrYxLf97974058 = EEQgnrYxLf65448423;     EEQgnrYxLf65448423 = EEQgnrYxLf33283701;     EEQgnrYxLf33283701 = EEQgnrYxLf90943640;     EEQgnrYxLf90943640 = EEQgnrYxLf67932589;     EEQgnrYxLf67932589 = EEQgnrYxLf52769166;     EEQgnrYxLf52769166 = EEQgnrYxLf67167363;     EEQgnrYxLf67167363 = EEQgnrYxLf74027187;     EEQgnrYxLf74027187 = EEQgnrYxLf30045073;     EEQgnrYxLf30045073 = EEQgnrYxLf12901161;     EEQgnrYxLf12901161 = EEQgnrYxLf26178850;     EEQgnrYxLf26178850 = EEQgnrYxLf3100840;     EEQgnrYxLf3100840 = EEQgnrYxLf59623599;     EEQgnrYxLf59623599 = EEQgnrYxLf230990;     EEQgnrYxLf230990 = EEQgnrYxLf50998816;     EEQgnrYxLf50998816 = EEQgnrYxLf25466507;     EEQgnrYxLf25466507 = EEQgnrYxLf85247518;     EEQgnrYxLf85247518 = EEQgnrYxLf97394027;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void AWlxtJraMh50263898() {     int bKwVxymItd52327637 = -638507705;    int bKwVxymItd10062923 = -634643566;    int bKwVxymItd84700002 = -780061796;    int bKwVxymItd29430731 = -260924852;    int bKwVxymItd39575503 = -749235389;    int bKwVxymItd14337705 = -310356832;    int bKwVxymItd3645538 = -395102892;    int bKwVxymItd72577819 = -980272961;    int bKwVxymItd26589963 = -616203338;    int bKwVxymItd68532417 = -249340570;    int bKwVxymItd69116433 = -156822490;    int bKwVxymItd29617588 = -462014691;    int bKwVxymItd2029641 = -819161730;    int bKwVxymItd37447363 = -995111533;    int bKwVxymItd68142937 = -741411912;    int bKwVxymItd40215700 = 88283823;    int bKwVxymItd3187038 = -655630888;    int bKwVxymItd14058888 = -288583180;    int bKwVxymItd79235587 = -74413582;    int bKwVxymItd64862295 = -96705325;    int bKwVxymItd54173616 = -374861539;    int bKwVxymItd91694741 = -852945140;    int bKwVxymItd57339165 = -135775122;    int bKwVxymItd23791755 = -720921484;    int bKwVxymItd51360280 = -674676132;    int bKwVxymItd70767172 = -146388896;    int bKwVxymItd68244907 = -78445807;    int bKwVxymItd81012471 = -272762134;    int bKwVxymItd79577288 = -941203448;    int bKwVxymItd4793849 = -884755373;    int bKwVxymItd23543456 = 39763408;    int bKwVxymItd91661498 = 78683787;    int bKwVxymItd88746288 = 48099799;    int bKwVxymItd9777012 = -531400107;    int bKwVxymItd36354742 = -507605805;    int bKwVxymItd45645899 = -867052883;    int bKwVxymItd11761470 = -701201258;    int bKwVxymItd98739917 = -734480534;    int bKwVxymItd8377343 = -939175591;    int bKwVxymItd9991622 = -175472328;    int bKwVxymItd38509288 = -913313203;    int bKwVxymItd26325101 = -792861219;    int bKwVxymItd7714347 = 18732774;    int bKwVxymItd53523164 = -500105401;    int bKwVxymItd79323349 = 89762803;    int bKwVxymItd73734236 = -515841751;    int bKwVxymItd25466850 = -621452835;    int bKwVxymItd21087003 = -777524337;    int bKwVxymItd89685688 = -823328958;    int bKwVxymItd51251014 = -285396757;    int bKwVxymItd67141954 = -69730669;    int bKwVxymItd60672191 = -244846558;    int bKwVxymItd71037332 = -980485865;    int bKwVxymItd61243612 = -709418457;    int bKwVxymItd96090282 = -504585900;    int bKwVxymItd60632896 = -785562566;    int bKwVxymItd52723757 = -398868444;    int bKwVxymItd60908248 = 40859687;    int bKwVxymItd78070450 = -586248720;    int bKwVxymItd68808331 = -502846493;    int bKwVxymItd46092797 = -131911026;    int bKwVxymItd22633066 = -22340758;    int bKwVxymItd93000531 = 60930486;    int bKwVxymItd21796115 = -731447965;    int bKwVxymItd44988962 = -189103979;    int bKwVxymItd77454934 = -135506277;    int bKwVxymItd40871300 = -410114491;    int bKwVxymItd92252628 = -187761623;    int bKwVxymItd1092621 = -387505728;    int bKwVxymItd22497039 = -874359030;    int bKwVxymItd28454231 = -210514919;    int bKwVxymItd4447121 = -921150354;    int bKwVxymItd5681546 = -349407590;    int bKwVxymItd69243965 = -898941255;    int bKwVxymItd26353007 = -183392122;    int bKwVxymItd27848516 = -582000321;    int bKwVxymItd83980394 = -771677915;    int bKwVxymItd3816002 = -635669722;    int bKwVxymItd44468405 = -710684287;    int bKwVxymItd77626044 = -58834382;    int bKwVxymItd45300323 = -524936062;    int bKwVxymItd47157904 = -300921470;    int bKwVxymItd91326783 = -449433177;    int bKwVxymItd28326275 = -555806692;    int bKwVxymItd37651895 = -715024705;    int bKwVxymItd62871264 = -715390035;    int bKwVxymItd20624166 = 59169651;    int bKwVxymItd27502676 = -242481745;    int bKwVxymItd13686730 = 73185792;    int bKwVxymItd75721846 = -722043240;    int bKwVxymItd92922141 = -368184440;    int bKwVxymItd50853222 = -642060946;    int bKwVxymItd20669467 = -48231814;    int bKwVxymItd39569012 = -336329098;    int bKwVxymItd63898824 = 56438698;    int bKwVxymItd15876223 = -790972446;    int bKwVxymItd33324570 = -753791706;    int bKwVxymItd85918232 = -249819261;    int bKwVxymItd8534202 = -211001423;    int bKwVxymItd1868415 = -638507705;     bKwVxymItd52327637 = bKwVxymItd10062923;     bKwVxymItd10062923 = bKwVxymItd84700002;     bKwVxymItd84700002 = bKwVxymItd29430731;     bKwVxymItd29430731 = bKwVxymItd39575503;     bKwVxymItd39575503 = bKwVxymItd14337705;     bKwVxymItd14337705 = bKwVxymItd3645538;     bKwVxymItd3645538 = bKwVxymItd72577819;     bKwVxymItd72577819 = bKwVxymItd26589963;     bKwVxymItd26589963 = bKwVxymItd68532417;     bKwVxymItd68532417 = bKwVxymItd69116433;     bKwVxymItd69116433 = bKwVxymItd29617588;     bKwVxymItd29617588 = bKwVxymItd2029641;     bKwVxymItd2029641 = bKwVxymItd37447363;     bKwVxymItd37447363 = bKwVxymItd68142937;     bKwVxymItd68142937 = bKwVxymItd40215700;     bKwVxymItd40215700 = bKwVxymItd3187038;     bKwVxymItd3187038 = bKwVxymItd14058888;     bKwVxymItd14058888 = bKwVxymItd79235587;     bKwVxymItd79235587 = bKwVxymItd64862295;     bKwVxymItd64862295 = bKwVxymItd54173616;     bKwVxymItd54173616 = bKwVxymItd91694741;     bKwVxymItd91694741 = bKwVxymItd57339165;     bKwVxymItd57339165 = bKwVxymItd23791755;     bKwVxymItd23791755 = bKwVxymItd51360280;     bKwVxymItd51360280 = bKwVxymItd70767172;     bKwVxymItd70767172 = bKwVxymItd68244907;     bKwVxymItd68244907 = bKwVxymItd81012471;     bKwVxymItd81012471 = bKwVxymItd79577288;     bKwVxymItd79577288 = bKwVxymItd4793849;     bKwVxymItd4793849 = bKwVxymItd23543456;     bKwVxymItd23543456 = bKwVxymItd91661498;     bKwVxymItd91661498 = bKwVxymItd88746288;     bKwVxymItd88746288 = bKwVxymItd9777012;     bKwVxymItd9777012 = bKwVxymItd36354742;     bKwVxymItd36354742 = bKwVxymItd45645899;     bKwVxymItd45645899 = bKwVxymItd11761470;     bKwVxymItd11761470 = bKwVxymItd98739917;     bKwVxymItd98739917 = bKwVxymItd8377343;     bKwVxymItd8377343 = bKwVxymItd9991622;     bKwVxymItd9991622 = bKwVxymItd38509288;     bKwVxymItd38509288 = bKwVxymItd26325101;     bKwVxymItd26325101 = bKwVxymItd7714347;     bKwVxymItd7714347 = bKwVxymItd53523164;     bKwVxymItd53523164 = bKwVxymItd79323349;     bKwVxymItd79323349 = bKwVxymItd73734236;     bKwVxymItd73734236 = bKwVxymItd25466850;     bKwVxymItd25466850 = bKwVxymItd21087003;     bKwVxymItd21087003 = bKwVxymItd89685688;     bKwVxymItd89685688 = bKwVxymItd51251014;     bKwVxymItd51251014 = bKwVxymItd67141954;     bKwVxymItd67141954 = bKwVxymItd60672191;     bKwVxymItd60672191 = bKwVxymItd71037332;     bKwVxymItd71037332 = bKwVxymItd61243612;     bKwVxymItd61243612 = bKwVxymItd96090282;     bKwVxymItd96090282 = bKwVxymItd60632896;     bKwVxymItd60632896 = bKwVxymItd52723757;     bKwVxymItd52723757 = bKwVxymItd60908248;     bKwVxymItd60908248 = bKwVxymItd78070450;     bKwVxymItd78070450 = bKwVxymItd68808331;     bKwVxymItd68808331 = bKwVxymItd46092797;     bKwVxymItd46092797 = bKwVxymItd22633066;     bKwVxymItd22633066 = bKwVxymItd93000531;     bKwVxymItd93000531 = bKwVxymItd21796115;     bKwVxymItd21796115 = bKwVxymItd44988962;     bKwVxymItd44988962 = bKwVxymItd77454934;     bKwVxymItd77454934 = bKwVxymItd40871300;     bKwVxymItd40871300 = bKwVxymItd92252628;     bKwVxymItd92252628 = bKwVxymItd1092621;     bKwVxymItd1092621 = bKwVxymItd22497039;     bKwVxymItd22497039 = bKwVxymItd28454231;     bKwVxymItd28454231 = bKwVxymItd4447121;     bKwVxymItd4447121 = bKwVxymItd5681546;     bKwVxymItd5681546 = bKwVxymItd69243965;     bKwVxymItd69243965 = bKwVxymItd26353007;     bKwVxymItd26353007 = bKwVxymItd27848516;     bKwVxymItd27848516 = bKwVxymItd83980394;     bKwVxymItd83980394 = bKwVxymItd3816002;     bKwVxymItd3816002 = bKwVxymItd44468405;     bKwVxymItd44468405 = bKwVxymItd77626044;     bKwVxymItd77626044 = bKwVxymItd45300323;     bKwVxymItd45300323 = bKwVxymItd47157904;     bKwVxymItd47157904 = bKwVxymItd91326783;     bKwVxymItd91326783 = bKwVxymItd28326275;     bKwVxymItd28326275 = bKwVxymItd37651895;     bKwVxymItd37651895 = bKwVxymItd62871264;     bKwVxymItd62871264 = bKwVxymItd20624166;     bKwVxymItd20624166 = bKwVxymItd27502676;     bKwVxymItd27502676 = bKwVxymItd13686730;     bKwVxymItd13686730 = bKwVxymItd75721846;     bKwVxymItd75721846 = bKwVxymItd92922141;     bKwVxymItd92922141 = bKwVxymItd50853222;     bKwVxymItd50853222 = bKwVxymItd20669467;     bKwVxymItd20669467 = bKwVxymItd39569012;     bKwVxymItd39569012 = bKwVxymItd63898824;     bKwVxymItd63898824 = bKwVxymItd15876223;     bKwVxymItd15876223 = bKwVxymItd33324570;     bKwVxymItd33324570 = bKwVxymItd85918232;     bKwVxymItd85918232 = bKwVxymItd8534202;     bKwVxymItd8534202 = bKwVxymItd1868415;     bKwVxymItd1868415 = bKwVxymItd52327637;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void lHUqzEXVVJ71199410() {     int kWxeEncwoK77919385 = -191016665;    int kWxeEncwoK67887749 = -847526217;    int kWxeEncwoK16265966 = -625394718;    int kWxeEncwoK90072800 = -447706915;    int kWxeEncwoK77406 = -327802355;    int kWxeEncwoK96541549 = -765905024;    int kWxeEncwoK40419047 = -172837632;    int kWxeEncwoK18373893 = -556777523;    int kWxeEncwoK69179091 = -298781312;    int kWxeEncwoK91051286 = -947095222;    int kWxeEncwoK63129381 = -507778305;    int kWxeEncwoK50643567 = -70245235;    int kWxeEncwoK46757504 = -561751620;    int kWxeEncwoK75162860 = -624482775;    int kWxeEncwoK79239716 = -667245018;    int kWxeEncwoK73372196 = -15112003;    int kWxeEncwoK47132394 = -524336112;    int kWxeEncwoK12174260 = -407788919;    int kWxeEncwoK20272039 = -239405172;    int kWxeEncwoK53949998 = -559120249;    int kWxeEncwoK75513457 = -26094946;    int kWxeEncwoK14901583 = -762686186;    int kWxeEncwoK24224567 = -820909385;    int kWxeEncwoK81855688 = -698849941;    int kWxeEncwoK77820730 = -915478422;    int kWxeEncwoK83682232 = -902633510;    int kWxeEncwoK80411235 = -443459229;    int kWxeEncwoK9507747 = -993457378;    int kWxeEncwoK74776054 = -531426984;    int kWxeEncwoK82026469 = -18233562;    int kWxeEncwoK12177412 = -451790388;    int kWxeEncwoK44127573 = -903290286;    int kWxeEncwoK24480917 = -197777122;    int kWxeEncwoK63081246 = -710863346;    int kWxeEncwoK55401692 = -949727792;    int kWxeEncwoK7609560 = -204881853;    int kWxeEncwoK49465487 = 17940176;    int kWxeEncwoK4394925 = -587932680;    int kWxeEncwoK13987325 = -684832497;    int kWxeEncwoK46902403 = 63989039;    int kWxeEncwoK97804901 = -300429495;    int kWxeEncwoK84637029 = -8107640;    int kWxeEncwoK96631206 = -550743353;    int kWxeEncwoK65084544 = -311556556;    int kWxeEncwoK87286173 = -475998913;    int kWxeEncwoK99706589 = -126344618;    int kWxeEncwoK97537511 = -172641073;    int kWxeEncwoK53816193 = -361945828;    int kWxeEncwoK21838620 = -259241323;    int kWxeEncwoK85015585 = -546395024;    int kWxeEncwoK64018831 = -739834264;    int kWxeEncwoK73383513 = -40779989;    int kWxeEncwoK13662588 = -556330405;    int kWxeEncwoK19892250 = -492562387;    int kWxeEncwoK95685953 = -122777040;    int kWxeEncwoK63017803 = -428330479;    int kWxeEncwoK43663183 = 73383168;    int kWxeEncwoK34410277 = -926544778;    int kWxeEncwoK12252070 = -532228494;    int kWxeEncwoK16395174 = -425168846;    int kWxeEncwoK16130315 = -222445795;    int kWxeEncwoK30911300 = -179380254;    int kWxeEncwoK43597839 = 74649460;    int kWxeEncwoK87152622 = -180547751;    int kWxeEncwoK78873875 = -395304835;    int kWxeEncwoK19001809 = -604488019;    int kWxeEncwoK26162650 = -872468114;    int kWxeEncwoK83676258 = -850888275;    int kWxeEncwoK19761168 = -674754983;    int kWxeEncwoK71630157 = -362363166;    int kWxeEncwoK23906709 = 66947821;    int kWxeEncwoK42737470 = -936403432;    int kWxeEncwoK98186934 = -722956423;    int kWxeEncwoK73369635 = -203394212;    int kWxeEncwoK56145097 = -158690755;    int kWxeEncwoK90876427 = 82012694;    int kWxeEncwoK18270377 = -111942834;    int kWxeEncwoK59140022 = -409352830;    int kWxeEncwoK94569515 = -122851028;    int kWxeEncwoK78114140 = -689133804;    int kWxeEncwoK86144721 = -629992437;    int kWxeEncwoK26595042 = 18486598;    int kWxeEncwoK87669127 = -634216056;    int kWxeEncwoK89760468 = -985031961;    int kWxeEncwoK18007638 = -278399298;    int kWxeEncwoK38793898 = -311010400;    int kWxeEncwoK30464985 = -246959881;    int kWxeEncwoK4588667 = -705214735;    int kWxeEncwoK67395293 = -488086307;    int kWxeEncwoK92383889 = -421397314;    int kWxeEncwoK63946377 = -178265021;    int kWxeEncwoK15055210 = -55515047;    int kWxeEncwoK92142854 = 44295814;    int kWxeEncwoK97592150 = -159663652;    int kWxeEncwoK30772089 = -713565166;    int kWxeEncwoK66893602 = -21049241;    int kWxeEncwoK41039190 = 17242899;    int kWxeEncwoK9478584 = -270195602;    int kWxeEncwoK86210669 = -916251722;    int kWxeEncwoK68284365 = -191016665;     kWxeEncwoK77919385 = kWxeEncwoK67887749;     kWxeEncwoK67887749 = kWxeEncwoK16265966;     kWxeEncwoK16265966 = kWxeEncwoK90072800;     kWxeEncwoK90072800 = kWxeEncwoK77406;     kWxeEncwoK77406 = kWxeEncwoK96541549;     kWxeEncwoK96541549 = kWxeEncwoK40419047;     kWxeEncwoK40419047 = kWxeEncwoK18373893;     kWxeEncwoK18373893 = kWxeEncwoK69179091;     kWxeEncwoK69179091 = kWxeEncwoK91051286;     kWxeEncwoK91051286 = kWxeEncwoK63129381;     kWxeEncwoK63129381 = kWxeEncwoK50643567;     kWxeEncwoK50643567 = kWxeEncwoK46757504;     kWxeEncwoK46757504 = kWxeEncwoK75162860;     kWxeEncwoK75162860 = kWxeEncwoK79239716;     kWxeEncwoK79239716 = kWxeEncwoK73372196;     kWxeEncwoK73372196 = kWxeEncwoK47132394;     kWxeEncwoK47132394 = kWxeEncwoK12174260;     kWxeEncwoK12174260 = kWxeEncwoK20272039;     kWxeEncwoK20272039 = kWxeEncwoK53949998;     kWxeEncwoK53949998 = kWxeEncwoK75513457;     kWxeEncwoK75513457 = kWxeEncwoK14901583;     kWxeEncwoK14901583 = kWxeEncwoK24224567;     kWxeEncwoK24224567 = kWxeEncwoK81855688;     kWxeEncwoK81855688 = kWxeEncwoK77820730;     kWxeEncwoK77820730 = kWxeEncwoK83682232;     kWxeEncwoK83682232 = kWxeEncwoK80411235;     kWxeEncwoK80411235 = kWxeEncwoK9507747;     kWxeEncwoK9507747 = kWxeEncwoK74776054;     kWxeEncwoK74776054 = kWxeEncwoK82026469;     kWxeEncwoK82026469 = kWxeEncwoK12177412;     kWxeEncwoK12177412 = kWxeEncwoK44127573;     kWxeEncwoK44127573 = kWxeEncwoK24480917;     kWxeEncwoK24480917 = kWxeEncwoK63081246;     kWxeEncwoK63081246 = kWxeEncwoK55401692;     kWxeEncwoK55401692 = kWxeEncwoK7609560;     kWxeEncwoK7609560 = kWxeEncwoK49465487;     kWxeEncwoK49465487 = kWxeEncwoK4394925;     kWxeEncwoK4394925 = kWxeEncwoK13987325;     kWxeEncwoK13987325 = kWxeEncwoK46902403;     kWxeEncwoK46902403 = kWxeEncwoK97804901;     kWxeEncwoK97804901 = kWxeEncwoK84637029;     kWxeEncwoK84637029 = kWxeEncwoK96631206;     kWxeEncwoK96631206 = kWxeEncwoK65084544;     kWxeEncwoK65084544 = kWxeEncwoK87286173;     kWxeEncwoK87286173 = kWxeEncwoK99706589;     kWxeEncwoK99706589 = kWxeEncwoK97537511;     kWxeEncwoK97537511 = kWxeEncwoK53816193;     kWxeEncwoK53816193 = kWxeEncwoK21838620;     kWxeEncwoK21838620 = kWxeEncwoK85015585;     kWxeEncwoK85015585 = kWxeEncwoK64018831;     kWxeEncwoK64018831 = kWxeEncwoK73383513;     kWxeEncwoK73383513 = kWxeEncwoK13662588;     kWxeEncwoK13662588 = kWxeEncwoK19892250;     kWxeEncwoK19892250 = kWxeEncwoK95685953;     kWxeEncwoK95685953 = kWxeEncwoK63017803;     kWxeEncwoK63017803 = kWxeEncwoK43663183;     kWxeEncwoK43663183 = kWxeEncwoK34410277;     kWxeEncwoK34410277 = kWxeEncwoK12252070;     kWxeEncwoK12252070 = kWxeEncwoK16395174;     kWxeEncwoK16395174 = kWxeEncwoK16130315;     kWxeEncwoK16130315 = kWxeEncwoK30911300;     kWxeEncwoK30911300 = kWxeEncwoK43597839;     kWxeEncwoK43597839 = kWxeEncwoK87152622;     kWxeEncwoK87152622 = kWxeEncwoK78873875;     kWxeEncwoK78873875 = kWxeEncwoK19001809;     kWxeEncwoK19001809 = kWxeEncwoK26162650;     kWxeEncwoK26162650 = kWxeEncwoK83676258;     kWxeEncwoK83676258 = kWxeEncwoK19761168;     kWxeEncwoK19761168 = kWxeEncwoK71630157;     kWxeEncwoK71630157 = kWxeEncwoK23906709;     kWxeEncwoK23906709 = kWxeEncwoK42737470;     kWxeEncwoK42737470 = kWxeEncwoK98186934;     kWxeEncwoK98186934 = kWxeEncwoK73369635;     kWxeEncwoK73369635 = kWxeEncwoK56145097;     kWxeEncwoK56145097 = kWxeEncwoK90876427;     kWxeEncwoK90876427 = kWxeEncwoK18270377;     kWxeEncwoK18270377 = kWxeEncwoK59140022;     kWxeEncwoK59140022 = kWxeEncwoK94569515;     kWxeEncwoK94569515 = kWxeEncwoK78114140;     kWxeEncwoK78114140 = kWxeEncwoK86144721;     kWxeEncwoK86144721 = kWxeEncwoK26595042;     kWxeEncwoK26595042 = kWxeEncwoK87669127;     kWxeEncwoK87669127 = kWxeEncwoK89760468;     kWxeEncwoK89760468 = kWxeEncwoK18007638;     kWxeEncwoK18007638 = kWxeEncwoK38793898;     kWxeEncwoK38793898 = kWxeEncwoK30464985;     kWxeEncwoK30464985 = kWxeEncwoK4588667;     kWxeEncwoK4588667 = kWxeEncwoK67395293;     kWxeEncwoK67395293 = kWxeEncwoK92383889;     kWxeEncwoK92383889 = kWxeEncwoK63946377;     kWxeEncwoK63946377 = kWxeEncwoK15055210;     kWxeEncwoK15055210 = kWxeEncwoK92142854;     kWxeEncwoK92142854 = kWxeEncwoK97592150;     kWxeEncwoK97592150 = kWxeEncwoK30772089;     kWxeEncwoK30772089 = kWxeEncwoK66893602;     kWxeEncwoK66893602 = kWxeEncwoK41039190;     kWxeEncwoK41039190 = kWxeEncwoK9478584;     kWxeEncwoK9478584 = kWxeEncwoK86210669;     kWxeEncwoK86210669 = kWxeEncwoK68284365;     kWxeEncwoK68284365 = kWxeEncwoK77919385;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void wtkPHSAKoa44377454() {     int pTTyAKuudn32852996 = -97900635;    int pTTyAKuudn95611375 = -131360695;    int pTTyAKuudn69176222 = 14438205;    int pTTyAKuudn95863443 = -264891419;    int pTTyAKuudn47969089 = -591318295;    int pTTyAKuudn23551063 = -119304876;    int pTTyAKuudn27360995 = -756333129;    int pTTyAKuudn76541637 = -590642313;    int pTTyAKuudn48643330 = -701658029;    int pTTyAKuudn87243208 = -980561395;    int pTTyAKuudn35213775 = -783304262;    int pTTyAKuudn40414652 = -198190349;    int pTTyAKuudn43510895 = -416462927;    int pTTyAKuudn82875543 = -797978858;    int pTTyAKuudn52788585 = -406040973;    int pTTyAKuudn48564599 = -881319893;    int pTTyAKuudn53077684 = -303470776;    int pTTyAKuudn12996670 = -934900084;    int pTTyAKuudn28978 = -315191088;    int pTTyAKuudn56324756 = 8806680;    int pTTyAKuudn21386968 = -579115587;    int pTTyAKuudn55008924 = -851133810;    int pTTyAKuudn26044556 = -297158346;    int pTTyAKuudn49720194 = -965298014;    int pTTyAKuudn68599481 = -807069876;    int pTTyAKuudn82573788 = -368799606;    int pTTyAKuudn54538358 = -714094589;    int pTTyAKuudn65624322 = -717995843;    int pTTyAKuudn50510779 = -473362617;    int pTTyAKuudn96176864 = -468748053;    int pTTyAKuudn27152050 = -482003734;    int pTTyAKuudn36806045 = -848788957;    int pTTyAKuudn62722608 = -698636650;    int pTTyAKuudn51303342 = -103478634;    int pTTyAKuudn78782444 = -704216924;    int pTTyAKuudn52408151 = -643250021;    int pTTyAKuudn55319386 = -273978561;    int pTTyAKuudn27175123 = -652534398;    int pTTyAKuudn76377614 = -87442182;    int pTTyAKuudn60582778 = -888372917;    int pTTyAKuudn84882899 = -491470806;    int pTTyAKuudn163629 = -747757075;    int pTTyAKuudn96864465 = -592595682;    int pTTyAKuudn29369523 = -707997183;    int pTTyAKuudn83312990 = -106373636;    int pTTyAKuudn59701077 = -890555001;    int pTTyAKuudn71174762 = -89040134;    int pTTyAKuudn96314754 = -102998379;    int pTTyAKuudn67208487 = -373153581;    int pTTyAKuudn30398095 = -111409298;    int pTTyAKuudn5965755 = -341920745;    int pTTyAKuudn58770589 = -249639447;    int pTTyAKuudn76660534 = -96430912;    int pTTyAKuudn98563854 = -146744672;    int pTTyAKuudn22990486 = -486926543;    int pTTyAKuudn77844071 = -246766825;    int pTTyAKuudn69566819 = -834202349;    int pTTyAKuudn19456029 = -20263781;    int pTTyAKuudn27263962 = -457821544;    int pTTyAKuudn65395301 = -122518689;    int pTTyAKuudn69012705 = -405210288;    int pTTyAKuudn61736673 = 61662714;    int pTTyAKuudn26030859 = -17279696;    int pTTyAKuudn52466465 = -132909976;    int pTTyAKuudn60091159 = -398557662;    int pTTyAKuudn98407729 = -934515306;    int pTTyAKuudn77692043 = -499553700;    int pTTyAKuudn92207552 = -212984294;    int pTTyAKuudn4093099 = 6238065;    int pTTyAKuudn380435 = -762790952;    int pTTyAKuudn93245212 = -507341333;    int pTTyAKuudn25902562 = -650936379;    int pTTyAKuudn36619055 = -747457903;    int pTTyAKuudn39446199 = -426818171;    int pTTyAKuudn71441857 = -499722514;    int pTTyAKuudn21223340 = -831358512;    int pTTyAKuudn58144459 = -158538129;    int pTTyAKuudn96675032 = -589161163;    int pTTyAKuudn66407203 = -758924379;    int pTTyAKuudn8898404 = -916514876;    int pTTyAKuudn11399026 = -179759473;    int pTTyAKuudn58223604 = -511096210;    int pTTyAKuudn98415834 = -244842263;    int pTTyAKuudn20112684 = -261953320;    int pTTyAKuudn90211109 = -26827308;    int pTTyAKuudn68381460 = -132364288;    int pTTyAKuudn60145510 = -652358045;    int pTTyAKuudn64158754 = -451891979;    int pTTyAKuudn28312857 = -616552091;    int pTTyAKuudn938374 = -357450100;    int pTTyAKuudn82841332 = -809047672;    int pTTyAKuudn35863358 = -153714781;    int pTTyAKuudn99911160 = -94712855;    int pTTyAKuudn10982314 = -964923494;    int pTTyAKuudn91570073 = -383162630;    int pTTyAKuudn23146226 = -453133521;    int pTTyAKuudn74132770 = -630477380;    int pTTyAKuudn44398000 = -359685706;    int pTTyAKuudn69278364 = -209439522;    int pTTyAKuudn84905261 = -97900635;     pTTyAKuudn32852996 = pTTyAKuudn95611375;     pTTyAKuudn95611375 = pTTyAKuudn69176222;     pTTyAKuudn69176222 = pTTyAKuudn95863443;     pTTyAKuudn95863443 = pTTyAKuudn47969089;     pTTyAKuudn47969089 = pTTyAKuudn23551063;     pTTyAKuudn23551063 = pTTyAKuudn27360995;     pTTyAKuudn27360995 = pTTyAKuudn76541637;     pTTyAKuudn76541637 = pTTyAKuudn48643330;     pTTyAKuudn48643330 = pTTyAKuudn87243208;     pTTyAKuudn87243208 = pTTyAKuudn35213775;     pTTyAKuudn35213775 = pTTyAKuudn40414652;     pTTyAKuudn40414652 = pTTyAKuudn43510895;     pTTyAKuudn43510895 = pTTyAKuudn82875543;     pTTyAKuudn82875543 = pTTyAKuudn52788585;     pTTyAKuudn52788585 = pTTyAKuudn48564599;     pTTyAKuudn48564599 = pTTyAKuudn53077684;     pTTyAKuudn53077684 = pTTyAKuudn12996670;     pTTyAKuudn12996670 = pTTyAKuudn28978;     pTTyAKuudn28978 = pTTyAKuudn56324756;     pTTyAKuudn56324756 = pTTyAKuudn21386968;     pTTyAKuudn21386968 = pTTyAKuudn55008924;     pTTyAKuudn55008924 = pTTyAKuudn26044556;     pTTyAKuudn26044556 = pTTyAKuudn49720194;     pTTyAKuudn49720194 = pTTyAKuudn68599481;     pTTyAKuudn68599481 = pTTyAKuudn82573788;     pTTyAKuudn82573788 = pTTyAKuudn54538358;     pTTyAKuudn54538358 = pTTyAKuudn65624322;     pTTyAKuudn65624322 = pTTyAKuudn50510779;     pTTyAKuudn50510779 = pTTyAKuudn96176864;     pTTyAKuudn96176864 = pTTyAKuudn27152050;     pTTyAKuudn27152050 = pTTyAKuudn36806045;     pTTyAKuudn36806045 = pTTyAKuudn62722608;     pTTyAKuudn62722608 = pTTyAKuudn51303342;     pTTyAKuudn51303342 = pTTyAKuudn78782444;     pTTyAKuudn78782444 = pTTyAKuudn52408151;     pTTyAKuudn52408151 = pTTyAKuudn55319386;     pTTyAKuudn55319386 = pTTyAKuudn27175123;     pTTyAKuudn27175123 = pTTyAKuudn76377614;     pTTyAKuudn76377614 = pTTyAKuudn60582778;     pTTyAKuudn60582778 = pTTyAKuudn84882899;     pTTyAKuudn84882899 = pTTyAKuudn163629;     pTTyAKuudn163629 = pTTyAKuudn96864465;     pTTyAKuudn96864465 = pTTyAKuudn29369523;     pTTyAKuudn29369523 = pTTyAKuudn83312990;     pTTyAKuudn83312990 = pTTyAKuudn59701077;     pTTyAKuudn59701077 = pTTyAKuudn71174762;     pTTyAKuudn71174762 = pTTyAKuudn96314754;     pTTyAKuudn96314754 = pTTyAKuudn67208487;     pTTyAKuudn67208487 = pTTyAKuudn30398095;     pTTyAKuudn30398095 = pTTyAKuudn5965755;     pTTyAKuudn5965755 = pTTyAKuudn58770589;     pTTyAKuudn58770589 = pTTyAKuudn76660534;     pTTyAKuudn76660534 = pTTyAKuudn98563854;     pTTyAKuudn98563854 = pTTyAKuudn22990486;     pTTyAKuudn22990486 = pTTyAKuudn77844071;     pTTyAKuudn77844071 = pTTyAKuudn69566819;     pTTyAKuudn69566819 = pTTyAKuudn19456029;     pTTyAKuudn19456029 = pTTyAKuudn27263962;     pTTyAKuudn27263962 = pTTyAKuudn65395301;     pTTyAKuudn65395301 = pTTyAKuudn69012705;     pTTyAKuudn69012705 = pTTyAKuudn61736673;     pTTyAKuudn61736673 = pTTyAKuudn26030859;     pTTyAKuudn26030859 = pTTyAKuudn52466465;     pTTyAKuudn52466465 = pTTyAKuudn60091159;     pTTyAKuudn60091159 = pTTyAKuudn98407729;     pTTyAKuudn98407729 = pTTyAKuudn77692043;     pTTyAKuudn77692043 = pTTyAKuudn92207552;     pTTyAKuudn92207552 = pTTyAKuudn4093099;     pTTyAKuudn4093099 = pTTyAKuudn380435;     pTTyAKuudn380435 = pTTyAKuudn93245212;     pTTyAKuudn93245212 = pTTyAKuudn25902562;     pTTyAKuudn25902562 = pTTyAKuudn36619055;     pTTyAKuudn36619055 = pTTyAKuudn39446199;     pTTyAKuudn39446199 = pTTyAKuudn71441857;     pTTyAKuudn71441857 = pTTyAKuudn21223340;     pTTyAKuudn21223340 = pTTyAKuudn58144459;     pTTyAKuudn58144459 = pTTyAKuudn96675032;     pTTyAKuudn96675032 = pTTyAKuudn66407203;     pTTyAKuudn66407203 = pTTyAKuudn8898404;     pTTyAKuudn8898404 = pTTyAKuudn11399026;     pTTyAKuudn11399026 = pTTyAKuudn58223604;     pTTyAKuudn58223604 = pTTyAKuudn98415834;     pTTyAKuudn98415834 = pTTyAKuudn20112684;     pTTyAKuudn20112684 = pTTyAKuudn90211109;     pTTyAKuudn90211109 = pTTyAKuudn68381460;     pTTyAKuudn68381460 = pTTyAKuudn60145510;     pTTyAKuudn60145510 = pTTyAKuudn64158754;     pTTyAKuudn64158754 = pTTyAKuudn28312857;     pTTyAKuudn28312857 = pTTyAKuudn938374;     pTTyAKuudn938374 = pTTyAKuudn82841332;     pTTyAKuudn82841332 = pTTyAKuudn35863358;     pTTyAKuudn35863358 = pTTyAKuudn99911160;     pTTyAKuudn99911160 = pTTyAKuudn10982314;     pTTyAKuudn10982314 = pTTyAKuudn91570073;     pTTyAKuudn91570073 = pTTyAKuudn23146226;     pTTyAKuudn23146226 = pTTyAKuudn74132770;     pTTyAKuudn74132770 = pTTyAKuudn44398000;     pTTyAKuudn44398000 = pTTyAKuudn69278364;     pTTyAKuudn69278364 = pTTyAKuudn84905261;     pTTyAKuudn84905261 = pTTyAKuudn32852996;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void AYQBtwfcrr65312966() {     int spAqKOMpMK58444744 = -750409594;    int spAqKOMpMK53436202 = -344243346;    int spAqKOMpMK742187 = -930894717;    int spAqKOMpMK56505512 = -451673483;    int spAqKOMpMK8470992 = -169885260;    int spAqKOMpMK5754908 = -574853067;    int spAqKOMpMK64134504 = -534067869;    int spAqKOMpMK22337711 = -167146875;    int spAqKOMpMK91232458 = -384236003;    int spAqKOMpMK9762077 = -578316048;    int spAqKOMpMK29226723 = -34260077;    int spAqKOMpMK61440631 = -906420893;    int spAqKOMpMK88238758 = -159052818;    int spAqKOMpMK20591040 = -427350100;    int spAqKOMpMK63885364 = -331874079;    int spAqKOMpMK81721094 = -984715720;    int spAqKOMpMK97023040 = -172176001;    int spAqKOMpMK11112041 = 45894177;    int spAqKOMpMK41065429 = -480182679;    int spAqKOMpMK45412460 = -453608244;    int spAqKOMpMK42726809 = -230348994;    int spAqKOMpMK78215765 = -760874857;    int spAqKOMpMK92929957 = -982292609;    int spAqKOMpMK7784129 = -943226472;    int spAqKOMpMK95059930 = 52127835;    int spAqKOMpMK95488847 = -25044220;    int spAqKOMpMK66704685 = 20891990;    int spAqKOMpMK94119597 = -338691087;    int spAqKOMpMK45709544 = -63586153;    int spAqKOMpMK73409485 = -702226241;    int spAqKOMpMK15786005 = -973557530;    int spAqKOMpMK89272120 = -730763029;    int spAqKOMpMK98457236 = -944513571;    int spAqKOMpMK4607577 = -282941873;    int spAqKOMpMK97829394 = -46338911;    int spAqKOMpMK14371812 = 18921009;    int spAqKOMpMK93023404 = -654837128;    int spAqKOMpMK32830130 = -505986544;    int spAqKOMpMK81987596 = -933099088;    int spAqKOMpMK97493559 = -648911550;    int spAqKOMpMK44178512 = -978587098;    int spAqKOMpMK58475557 = 36996503;    int spAqKOMpMK85781325 = -62071809;    int spAqKOMpMK40930904 = -519448339;    int spAqKOMpMK91275814 = -672135352;    int spAqKOMpMK85673430 = -501057868;    int spAqKOMpMK43245424 = -740228372;    int spAqKOMpMK29043944 = -787419870;    int spAqKOMpMK99361417 = -909065946;    int spAqKOMpMK64162666 = -372407565;    int spAqKOMpMK2842633 = 87975660;    int spAqKOMpMK71481911 = -45572878;    int spAqKOMpMK19285790 = -772275452;    int spAqKOMpMK57212492 = 70111398;    int spAqKOMpMK22586157 = -105117682;    int spAqKOMpMK80228978 = -989534738;    int spAqKOMpMK60506245 = -361950738;    int spAqKOMpMK92958057 = -987668246;    int spAqKOMpMK61445581 = -403801318;    int spAqKOMpMK12982145 = -44841041;    int spAqKOMpMK39050222 = -495745058;    int spAqKOMpMK70014907 = -95376782;    int spAqKOMpMK76628166 = -3560722;    int spAqKOMpMK17822974 = -682009763;    int spAqKOMpMK93976071 = -604758518;    int spAqKOMpMK39954603 = -303497048;    int spAqKOMpMK62983394 = -961907323;    int spAqKOMpMK83631182 = -876110946;    int spAqKOMpMK22761646 = -281011190;    int spAqKOMpMK49513552 = -250795088;    int spAqKOMpMK88697690 = -229878593;    int spAqKOMpMK64192911 = -666189457;    int spAqKOMpMK29124445 = -21006736;    int spAqKOMpMK43571870 = -831271129;    int spAqKOMpMK1233948 = -475021147;    int spAqKOMpMK84251252 = -167345498;    int spAqKOMpMK92434440 = -598803048;    int spAqKOMpMK51999053 = -362844271;    int spAqKOMpMK16508314 = -171091120;    int spAqKOMpMK9386500 = -446814298;    int spAqKOMpMK52243424 = -284815848;    int spAqKOMpMK37660742 = -191688141;    int spAqKOMpMK94758179 = -429625142;    int spAqKOMpMK81546877 = -691178589;    int spAqKOMpMK70566853 = -690201902;    int spAqKOMpMK44304094 = -827984653;    int spAqKOMpMK69986330 = -958487577;    int spAqKOMpMK41244744 = -914624970;    int spAqKOMpMK82021419 = -77824191;    int spAqKOMpMK17600416 = -56804173;    int spAqKOMpMK53865567 = -619128254;    int spAqKOMpMK65347 = -667168882;    int spAqKOMpMK71384548 = -2185227;    int spAqKOMpMK69005452 = -788258047;    int spAqKOMpMK58443338 = -53166493;    int spAqKOMpMK74163605 = -783210316;    int spAqKOMpMK81847391 = -959442775;    int spAqKOMpMK67958352 = -380062047;    int spAqKOMpMK46954832 = -914689821;    int spAqKOMpMK51321211 = -750409594;     spAqKOMpMK58444744 = spAqKOMpMK53436202;     spAqKOMpMK53436202 = spAqKOMpMK742187;     spAqKOMpMK742187 = spAqKOMpMK56505512;     spAqKOMpMK56505512 = spAqKOMpMK8470992;     spAqKOMpMK8470992 = spAqKOMpMK5754908;     spAqKOMpMK5754908 = spAqKOMpMK64134504;     spAqKOMpMK64134504 = spAqKOMpMK22337711;     spAqKOMpMK22337711 = spAqKOMpMK91232458;     spAqKOMpMK91232458 = spAqKOMpMK9762077;     spAqKOMpMK9762077 = spAqKOMpMK29226723;     spAqKOMpMK29226723 = spAqKOMpMK61440631;     spAqKOMpMK61440631 = spAqKOMpMK88238758;     spAqKOMpMK88238758 = spAqKOMpMK20591040;     spAqKOMpMK20591040 = spAqKOMpMK63885364;     spAqKOMpMK63885364 = spAqKOMpMK81721094;     spAqKOMpMK81721094 = spAqKOMpMK97023040;     spAqKOMpMK97023040 = spAqKOMpMK11112041;     spAqKOMpMK11112041 = spAqKOMpMK41065429;     spAqKOMpMK41065429 = spAqKOMpMK45412460;     spAqKOMpMK45412460 = spAqKOMpMK42726809;     spAqKOMpMK42726809 = spAqKOMpMK78215765;     spAqKOMpMK78215765 = spAqKOMpMK92929957;     spAqKOMpMK92929957 = spAqKOMpMK7784129;     spAqKOMpMK7784129 = spAqKOMpMK95059930;     spAqKOMpMK95059930 = spAqKOMpMK95488847;     spAqKOMpMK95488847 = spAqKOMpMK66704685;     spAqKOMpMK66704685 = spAqKOMpMK94119597;     spAqKOMpMK94119597 = spAqKOMpMK45709544;     spAqKOMpMK45709544 = spAqKOMpMK73409485;     spAqKOMpMK73409485 = spAqKOMpMK15786005;     spAqKOMpMK15786005 = spAqKOMpMK89272120;     spAqKOMpMK89272120 = spAqKOMpMK98457236;     spAqKOMpMK98457236 = spAqKOMpMK4607577;     spAqKOMpMK4607577 = spAqKOMpMK97829394;     spAqKOMpMK97829394 = spAqKOMpMK14371812;     spAqKOMpMK14371812 = spAqKOMpMK93023404;     spAqKOMpMK93023404 = spAqKOMpMK32830130;     spAqKOMpMK32830130 = spAqKOMpMK81987596;     spAqKOMpMK81987596 = spAqKOMpMK97493559;     spAqKOMpMK97493559 = spAqKOMpMK44178512;     spAqKOMpMK44178512 = spAqKOMpMK58475557;     spAqKOMpMK58475557 = spAqKOMpMK85781325;     spAqKOMpMK85781325 = spAqKOMpMK40930904;     spAqKOMpMK40930904 = spAqKOMpMK91275814;     spAqKOMpMK91275814 = spAqKOMpMK85673430;     spAqKOMpMK85673430 = spAqKOMpMK43245424;     spAqKOMpMK43245424 = spAqKOMpMK29043944;     spAqKOMpMK29043944 = spAqKOMpMK99361417;     spAqKOMpMK99361417 = spAqKOMpMK64162666;     spAqKOMpMK64162666 = spAqKOMpMK2842633;     spAqKOMpMK2842633 = spAqKOMpMK71481911;     spAqKOMpMK71481911 = spAqKOMpMK19285790;     spAqKOMpMK19285790 = spAqKOMpMK57212492;     spAqKOMpMK57212492 = spAqKOMpMK22586157;     spAqKOMpMK22586157 = spAqKOMpMK80228978;     spAqKOMpMK80228978 = spAqKOMpMK60506245;     spAqKOMpMK60506245 = spAqKOMpMK92958057;     spAqKOMpMK92958057 = spAqKOMpMK61445581;     spAqKOMpMK61445581 = spAqKOMpMK12982145;     spAqKOMpMK12982145 = spAqKOMpMK39050222;     spAqKOMpMK39050222 = spAqKOMpMK70014907;     spAqKOMpMK70014907 = spAqKOMpMK76628166;     spAqKOMpMK76628166 = spAqKOMpMK17822974;     spAqKOMpMK17822974 = spAqKOMpMK93976071;     spAqKOMpMK93976071 = spAqKOMpMK39954603;     spAqKOMpMK39954603 = spAqKOMpMK62983394;     spAqKOMpMK62983394 = spAqKOMpMK83631182;     spAqKOMpMK83631182 = spAqKOMpMK22761646;     spAqKOMpMK22761646 = spAqKOMpMK49513552;     spAqKOMpMK49513552 = spAqKOMpMK88697690;     spAqKOMpMK88697690 = spAqKOMpMK64192911;     spAqKOMpMK64192911 = spAqKOMpMK29124445;     spAqKOMpMK29124445 = spAqKOMpMK43571870;     spAqKOMpMK43571870 = spAqKOMpMK1233948;     spAqKOMpMK1233948 = spAqKOMpMK84251252;     spAqKOMpMK84251252 = spAqKOMpMK92434440;     spAqKOMpMK92434440 = spAqKOMpMK51999053;     spAqKOMpMK51999053 = spAqKOMpMK16508314;     spAqKOMpMK16508314 = spAqKOMpMK9386500;     spAqKOMpMK9386500 = spAqKOMpMK52243424;     spAqKOMpMK52243424 = spAqKOMpMK37660742;     spAqKOMpMK37660742 = spAqKOMpMK94758179;     spAqKOMpMK94758179 = spAqKOMpMK81546877;     spAqKOMpMK81546877 = spAqKOMpMK70566853;     spAqKOMpMK70566853 = spAqKOMpMK44304094;     spAqKOMpMK44304094 = spAqKOMpMK69986330;     spAqKOMpMK69986330 = spAqKOMpMK41244744;     spAqKOMpMK41244744 = spAqKOMpMK82021419;     spAqKOMpMK82021419 = spAqKOMpMK17600416;     spAqKOMpMK17600416 = spAqKOMpMK53865567;     spAqKOMpMK53865567 = spAqKOMpMK65347;     spAqKOMpMK65347 = spAqKOMpMK71384548;     spAqKOMpMK71384548 = spAqKOMpMK69005452;     spAqKOMpMK69005452 = spAqKOMpMK58443338;     spAqKOMpMK58443338 = spAqKOMpMK74163605;     spAqKOMpMK74163605 = spAqKOMpMK81847391;     spAqKOMpMK81847391 = spAqKOMpMK67958352;     spAqKOMpMK67958352 = spAqKOMpMK46954832;     spAqKOMpMK46954832 = spAqKOMpMK51321211;     spAqKOMpMK51321211 = spAqKOMpMK58444744;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void dbZpbvpfJJ38491010() {     int tBapNnFagd13378354 = -657293564;    int tBapNnFagd81159827 = -728077824;    int tBapNnFagd53652443 = -291061793;    int tBapNnFagd62296155 = -268857987;    int tBapNnFagd56362675 = -433401200;    int tBapNnFagd32764421 = 71747081;    int tBapNnFagd51076453 = -17563366;    int tBapNnFagd80505454 = -201011664;    int tBapNnFagd70696697 = -787112719;    int tBapNnFagd5953999 = -611782220;    int tBapNnFagd1311117 = -309786034;    int tBapNnFagd51211715 = 65633993;    int tBapNnFagd84992149 = -13764125;    int tBapNnFagd28303723 = -600846184;    int tBapNnFagd37434233 = -70670033;    int tBapNnFagd56913497 = -750923610;    int tBapNnFagd2968331 = 48689335;    int tBapNnFagd11934452 = -481216988;    int tBapNnFagd20822369 = -555968594;    int tBapNnFagd47787217 = -985681315;    int tBapNnFagd88600320 = -783369635;    int tBapNnFagd18323107 = -849322480;    int tBapNnFagd94749946 = -458541570;    int tBapNnFagd75648634 = -109674545;    int tBapNnFagd85838681 = -939463620;    int tBapNnFagd94380403 = -591210317;    int tBapNnFagd40831809 = -249743370;    int tBapNnFagd50236172 = -63229552;    int tBapNnFagd21444269 = -5521786;    int tBapNnFagd87559880 = -52740732;    int tBapNnFagd30760643 = 96229125;    int tBapNnFagd81950591 = -676261700;    int tBapNnFagd36698928 = -345373099;    int tBapNnFagd92829672 = -775557160;    int tBapNnFagd21210146 = -900828043;    int tBapNnFagd59170403 = -419447159;    int tBapNnFagd98877303 = -946755865;    int tBapNnFagd55610328 = -570588262;    int tBapNnFagd44377886 = -335708773;    int tBapNnFagd11173935 = -501273507;    int tBapNnFagd31256510 = -69628409;    int tBapNnFagd74002156 = -702652931;    int tBapNnFagd86014584 = -103924139;    int tBapNnFagd5215883 = -915888966;    int tBapNnFagd87302632 = -302510074;    int tBapNnFagd45667918 = -165268250;    int tBapNnFagd16882675 = -656627434;    int tBapNnFagd71542505 = -528472421;    int tBapNnFagd44731285 = 77021797;    int tBapNnFagd9545176 = 62578161;    int tBapNnFagd44789555 = -614110821;    int tBapNnFagd56868986 = -254432335;    int tBapNnFagd82283736 = -312375959;    int tBapNnFagd35884097 = -684070887;    int tBapNnFagd49890689 = -469267186;    int tBapNnFagd95055246 = -807971084;    int tBapNnFagd86409881 = -169536255;    int tBapNnFagd78003808 = -81387249;    int tBapNnFagd76457473 = -329394367;    int tBapNnFagd61982271 = -842190884;    int tBapNnFagd91932612 = -678509550;    int tBapNnFagd840281 = -954333814;    int tBapNnFagd59061185 = -95489878;    int tBapNnFagd83136816 = -634371988;    int tBapNnFagd75193355 = -608011346;    int tBapNnFagd19360525 = -633524334;    int tBapNnFagd14512788 = -588992908;    int tBapNnFagd92162476 = -238206966;    int tBapNnFagd7093578 = -700018142;    int tBapNnFagd78263829 = -651222875;    int tBapNnFagd58036193 = -804167746;    int tBapNnFagd47358002 = -380722404;    int tBapNnFagd67556565 = -45508215;    int tBapNnFagd9648434 = 45304912;    int tBapNnFagd16530708 = -816052907;    int tBapNnFagd14598164 = 19283296;    int tBapNnFagd32308523 = -645398342;    int tBapNnFagd89534063 = -542652604;    int tBapNnFagd88346002 = -807164471;    int tBapNnFagd40170763 = -674195370;    int tBapNnFagd77497729 = -934582883;    int tBapNnFagd69289304 = -721270949;    int tBapNnFagd5504887 = -40251350;    int tBapNnFagd11899094 = 31900052;    int tBapNnFagd42770325 = -438629912;    int tBapNnFagd73891657 = -649338540;    int tBapNnFagd99666855 = -263885741;    int tBapNnFagd814832 = -661302213;    int tBapNnFagd42938983 = -206289975;    int tBapNnFagd26154900 = 7143041;    int tBapNnFagd72760522 = -149910905;    int tBapNnFagd20873495 = -765368616;    int tBapNnFagd79152854 = -141193895;    int tBapNnFagd82395615 = -493517889;    int tBapNnFagd19241323 = -822763957;    int tBapNnFagd30416229 = -115294596;    int tBapNnFagd14940971 = -507163054;    int tBapNnFagd2877768 = -469552152;    int tBapNnFagd30022527 = -207877621;    int tBapNnFagd67942107 = -657293564;     tBapNnFagd13378354 = tBapNnFagd81159827;     tBapNnFagd81159827 = tBapNnFagd53652443;     tBapNnFagd53652443 = tBapNnFagd62296155;     tBapNnFagd62296155 = tBapNnFagd56362675;     tBapNnFagd56362675 = tBapNnFagd32764421;     tBapNnFagd32764421 = tBapNnFagd51076453;     tBapNnFagd51076453 = tBapNnFagd80505454;     tBapNnFagd80505454 = tBapNnFagd70696697;     tBapNnFagd70696697 = tBapNnFagd5953999;     tBapNnFagd5953999 = tBapNnFagd1311117;     tBapNnFagd1311117 = tBapNnFagd51211715;     tBapNnFagd51211715 = tBapNnFagd84992149;     tBapNnFagd84992149 = tBapNnFagd28303723;     tBapNnFagd28303723 = tBapNnFagd37434233;     tBapNnFagd37434233 = tBapNnFagd56913497;     tBapNnFagd56913497 = tBapNnFagd2968331;     tBapNnFagd2968331 = tBapNnFagd11934452;     tBapNnFagd11934452 = tBapNnFagd20822369;     tBapNnFagd20822369 = tBapNnFagd47787217;     tBapNnFagd47787217 = tBapNnFagd88600320;     tBapNnFagd88600320 = tBapNnFagd18323107;     tBapNnFagd18323107 = tBapNnFagd94749946;     tBapNnFagd94749946 = tBapNnFagd75648634;     tBapNnFagd75648634 = tBapNnFagd85838681;     tBapNnFagd85838681 = tBapNnFagd94380403;     tBapNnFagd94380403 = tBapNnFagd40831809;     tBapNnFagd40831809 = tBapNnFagd50236172;     tBapNnFagd50236172 = tBapNnFagd21444269;     tBapNnFagd21444269 = tBapNnFagd87559880;     tBapNnFagd87559880 = tBapNnFagd30760643;     tBapNnFagd30760643 = tBapNnFagd81950591;     tBapNnFagd81950591 = tBapNnFagd36698928;     tBapNnFagd36698928 = tBapNnFagd92829672;     tBapNnFagd92829672 = tBapNnFagd21210146;     tBapNnFagd21210146 = tBapNnFagd59170403;     tBapNnFagd59170403 = tBapNnFagd98877303;     tBapNnFagd98877303 = tBapNnFagd55610328;     tBapNnFagd55610328 = tBapNnFagd44377886;     tBapNnFagd44377886 = tBapNnFagd11173935;     tBapNnFagd11173935 = tBapNnFagd31256510;     tBapNnFagd31256510 = tBapNnFagd74002156;     tBapNnFagd74002156 = tBapNnFagd86014584;     tBapNnFagd86014584 = tBapNnFagd5215883;     tBapNnFagd5215883 = tBapNnFagd87302632;     tBapNnFagd87302632 = tBapNnFagd45667918;     tBapNnFagd45667918 = tBapNnFagd16882675;     tBapNnFagd16882675 = tBapNnFagd71542505;     tBapNnFagd71542505 = tBapNnFagd44731285;     tBapNnFagd44731285 = tBapNnFagd9545176;     tBapNnFagd9545176 = tBapNnFagd44789555;     tBapNnFagd44789555 = tBapNnFagd56868986;     tBapNnFagd56868986 = tBapNnFagd82283736;     tBapNnFagd82283736 = tBapNnFagd35884097;     tBapNnFagd35884097 = tBapNnFagd49890689;     tBapNnFagd49890689 = tBapNnFagd95055246;     tBapNnFagd95055246 = tBapNnFagd86409881;     tBapNnFagd86409881 = tBapNnFagd78003808;     tBapNnFagd78003808 = tBapNnFagd76457473;     tBapNnFagd76457473 = tBapNnFagd61982271;     tBapNnFagd61982271 = tBapNnFagd91932612;     tBapNnFagd91932612 = tBapNnFagd840281;     tBapNnFagd840281 = tBapNnFagd59061185;     tBapNnFagd59061185 = tBapNnFagd83136816;     tBapNnFagd83136816 = tBapNnFagd75193355;     tBapNnFagd75193355 = tBapNnFagd19360525;     tBapNnFagd19360525 = tBapNnFagd14512788;     tBapNnFagd14512788 = tBapNnFagd92162476;     tBapNnFagd92162476 = tBapNnFagd7093578;     tBapNnFagd7093578 = tBapNnFagd78263829;     tBapNnFagd78263829 = tBapNnFagd58036193;     tBapNnFagd58036193 = tBapNnFagd47358002;     tBapNnFagd47358002 = tBapNnFagd67556565;     tBapNnFagd67556565 = tBapNnFagd9648434;     tBapNnFagd9648434 = tBapNnFagd16530708;     tBapNnFagd16530708 = tBapNnFagd14598164;     tBapNnFagd14598164 = tBapNnFagd32308523;     tBapNnFagd32308523 = tBapNnFagd89534063;     tBapNnFagd89534063 = tBapNnFagd88346002;     tBapNnFagd88346002 = tBapNnFagd40170763;     tBapNnFagd40170763 = tBapNnFagd77497729;     tBapNnFagd77497729 = tBapNnFagd69289304;     tBapNnFagd69289304 = tBapNnFagd5504887;     tBapNnFagd5504887 = tBapNnFagd11899094;     tBapNnFagd11899094 = tBapNnFagd42770325;     tBapNnFagd42770325 = tBapNnFagd73891657;     tBapNnFagd73891657 = tBapNnFagd99666855;     tBapNnFagd99666855 = tBapNnFagd814832;     tBapNnFagd814832 = tBapNnFagd42938983;     tBapNnFagd42938983 = tBapNnFagd26154900;     tBapNnFagd26154900 = tBapNnFagd72760522;     tBapNnFagd72760522 = tBapNnFagd20873495;     tBapNnFagd20873495 = tBapNnFagd79152854;     tBapNnFagd79152854 = tBapNnFagd82395615;     tBapNnFagd82395615 = tBapNnFagd19241323;     tBapNnFagd19241323 = tBapNnFagd30416229;     tBapNnFagd30416229 = tBapNnFagd14940971;     tBapNnFagd14940971 = tBapNnFagd2877768;     tBapNnFagd2877768 = tBapNnFagd30022527;     tBapNnFagd30022527 = tBapNnFagd67942107;     tBapNnFagd67942107 = tBapNnFagd13378354;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void eVvmZQLRGU59426522() {     int kvWPgVocFK38970102 = -209802524;    int kvWPgVocFK38984655 = -940960475;    int kvWPgVocFK85218406 = -136394716;    int kvWPgVocFK22938224 = -455640050;    int kvWPgVocFK16864578 = -11968166;    int kvWPgVocFK14968266 = -383801111;    int kvWPgVocFK87849961 = -895298106;    int kvWPgVocFK26301528 = -877516226;    int kvWPgVocFK13285826 = -469690694;    int kvWPgVocFK28472868 = -209536873;    int kvWPgVocFK95324064 = -660741849;    int kvWPgVocFK72237694 = -642596551;    int kvWPgVocFK29720013 = -856354016;    int kvWPgVocFK66019220 = -230217426;    int kvWPgVocFK48531012 = 3496861;    int kvWPgVocFK90069992 = -854319437;    int kvWPgVocFK46913687 = -920015889;    int kvWPgVocFK10049823 = -600422727;    int kvWPgVocFK61858819 = -720960185;    int kvWPgVocFK36874921 = -348096239;    int kvWPgVocFK9940162 = -434603042;    int kvWPgVocFK41529948 = -759063527;    int kvWPgVocFK61635348 = -43675833;    int kvWPgVocFK33712568 = -87603002;    int kvWPgVocFK12299131 = -80265909;    int kvWPgVocFK7295464 = -247454930;    int kvWPgVocFK52998136 = -614756792;    int kvWPgVocFK78731447 = -783924797;    int kvWPgVocFK16643034 = -695745322;    int kvWPgVocFK64792501 = -286218920;    int kvWPgVocFK19394599 = -395324672;    int kvWPgVocFK34416667 = -558235772;    int kvWPgVocFK72433556 = -591250020;    int kvWPgVocFK46133907 = -955020399;    int kvWPgVocFK40257096 = -242950029;    int kvWPgVocFK21134065 = -857276129;    int kvWPgVocFK36581321 = -227614431;    int kvWPgVocFK61265335 = -424040408;    int kvWPgVocFK49987868 = -81365679;    int kvWPgVocFK48084716 = -261812140;    int kvWPgVocFK90552122 = -556744700;    int kvWPgVocFK32314085 = 82100647;    int kvWPgVocFK74931444 = -673400266;    int kvWPgVocFK16777264 = -727340122;    int kvWPgVocFK95265455 = -868271790;    int kvWPgVocFK71640272 = -875771117;    int kvWPgVocFK88953336 = -207815672;    int kvWPgVocFK4271695 = -112893912;    int kvWPgVocFK76884216 = -458890569;    int kvWPgVocFK43309747 = -198420106;    int kvWPgVocFK41666433 = -184214416;    int kvWPgVocFK69580308 = -50365767;    int kvWPgVocFK24908992 = -988220500;    int kvWPgVocFK94532734 = -467214817;    int kvWPgVocFK49486360 = -87458325;    int kvWPgVocFK97440154 = -450738997;    int kvWPgVocFK77349306 = -797284643;    int kvWPgVocFK51505838 = 51208286;    int kvWPgVocFK10639093 = -275374141;    int kvWPgVocFK9569115 = -764513237;    int kvWPgVocFK61970129 = -769044320;    int kvWPgVocFK9118515 = -11373310;    int kvWPgVocFK9658494 = -81770904;    int kvWPgVocFK48493324 = -83471774;    int kvWPgVocFK9078269 = -814212202;    int kvWPgVocFK60907398 = -2506077;    int kvWPgVocFK99804137 = 48653469;    int kvWPgVocFK83586106 = -901333618;    int kvWPgVocFK25762124 = -987267397;    int kvWPgVocFK27396948 = -139227011;    int kvWPgVocFK53488671 = -526705006;    int kvWPgVocFK85648352 = -395975482;    int kvWPgVocFK60061954 = -419057048;    int kvWPgVocFK13774104 = -359148045;    int kvWPgVocFK46322798 = -791351540;    int kvWPgVocFK77626076 = -416703690;    int kvWPgVocFK66598504 = 14336739;    int kvWPgVocFK44858084 = -316335712;    int kvWPgVocFK38447112 = -219331213;    int kvWPgVocFK40658859 = -204494792;    int kvWPgVocFK18342127 = 60360742;    int kvWPgVocFK48726441 = -401862880;    int kvWPgVocFK1847232 = -225034229;    int kvWPgVocFK73333287 = -397325216;    int kvWPgVocFK23126068 = -2004505;    int kvWPgVocFK49814290 = -244958906;    int kvWPgVocFK9507675 = -570015273;    int kvWPgVocFK77900822 = -24035204;    int kvWPgVocFK96647546 = -767562074;    int kvWPgVocFK42816942 = -792211033;    int kvWPgVocFK43784758 = 40008514;    int kvWPgVocFK85075483 = -178822717;    int kvWPgVocFK50626242 = -48666267;    int kvWPgVocFK40418754 = -316852443;    int kvWPgVocFK86114587 = -492767820;    int kvWPgVocFK81433608 = -445371391;    int kvWPgVocFK22655592 = -836128449;    int kvWPgVocFK26438120 = -489928492;    int kvWPgVocFK7698995 = -913127921;    int kvWPgVocFK34358058 = -209802524;     kvWPgVocFK38970102 = kvWPgVocFK38984655;     kvWPgVocFK38984655 = kvWPgVocFK85218406;     kvWPgVocFK85218406 = kvWPgVocFK22938224;     kvWPgVocFK22938224 = kvWPgVocFK16864578;     kvWPgVocFK16864578 = kvWPgVocFK14968266;     kvWPgVocFK14968266 = kvWPgVocFK87849961;     kvWPgVocFK87849961 = kvWPgVocFK26301528;     kvWPgVocFK26301528 = kvWPgVocFK13285826;     kvWPgVocFK13285826 = kvWPgVocFK28472868;     kvWPgVocFK28472868 = kvWPgVocFK95324064;     kvWPgVocFK95324064 = kvWPgVocFK72237694;     kvWPgVocFK72237694 = kvWPgVocFK29720013;     kvWPgVocFK29720013 = kvWPgVocFK66019220;     kvWPgVocFK66019220 = kvWPgVocFK48531012;     kvWPgVocFK48531012 = kvWPgVocFK90069992;     kvWPgVocFK90069992 = kvWPgVocFK46913687;     kvWPgVocFK46913687 = kvWPgVocFK10049823;     kvWPgVocFK10049823 = kvWPgVocFK61858819;     kvWPgVocFK61858819 = kvWPgVocFK36874921;     kvWPgVocFK36874921 = kvWPgVocFK9940162;     kvWPgVocFK9940162 = kvWPgVocFK41529948;     kvWPgVocFK41529948 = kvWPgVocFK61635348;     kvWPgVocFK61635348 = kvWPgVocFK33712568;     kvWPgVocFK33712568 = kvWPgVocFK12299131;     kvWPgVocFK12299131 = kvWPgVocFK7295464;     kvWPgVocFK7295464 = kvWPgVocFK52998136;     kvWPgVocFK52998136 = kvWPgVocFK78731447;     kvWPgVocFK78731447 = kvWPgVocFK16643034;     kvWPgVocFK16643034 = kvWPgVocFK64792501;     kvWPgVocFK64792501 = kvWPgVocFK19394599;     kvWPgVocFK19394599 = kvWPgVocFK34416667;     kvWPgVocFK34416667 = kvWPgVocFK72433556;     kvWPgVocFK72433556 = kvWPgVocFK46133907;     kvWPgVocFK46133907 = kvWPgVocFK40257096;     kvWPgVocFK40257096 = kvWPgVocFK21134065;     kvWPgVocFK21134065 = kvWPgVocFK36581321;     kvWPgVocFK36581321 = kvWPgVocFK61265335;     kvWPgVocFK61265335 = kvWPgVocFK49987868;     kvWPgVocFK49987868 = kvWPgVocFK48084716;     kvWPgVocFK48084716 = kvWPgVocFK90552122;     kvWPgVocFK90552122 = kvWPgVocFK32314085;     kvWPgVocFK32314085 = kvWPgVocFK74931444;     kvWPgVocFK74931444 = kvWPgVocFK16777264;     kvWPgVocFK16777264 = kvWPgVocFK95265455;     kvWPgVocFK95265455 = kvWPgVocFK71640272;     kvWPgVocFK71640272 = kvWPgVocFK88953336;     kvWPgVocFK88953336 = kvWPgVocFK4271695;     kvWPgVocFK4271695 = kvWPgVocFK76884216;     kvWPgVocFK76884216 = kvWPgVocFK43309747;     kvWPgVocFK43309747 = kvWPgVocFK41666433;     kvWPgVocFK41666433 = kvWPgVocFK69580308;     kvWPgVocFK69580308 = kvWPgVocFK24908992;     kvWPgVocFK24908992 = kvWPgVocFK94532734;     kvWPgVocFK94532734 = kvWPgVocFK49486360;     kvWPgVocFK49486360 = kvWPgVocFK97440154;     kvWPgVocFK97440154 = kvWPgVocFK77349306;     kvWPgVocFK77349306 = kvWPgVocFK51505838;     kvWPgVocFK51505838 = kvWPgVocFK10639093;     kvWPgVocFK10639093 = kvWPgVocFK9569115;     kvWPgVocFK9569115 = kvWPgVocFK61970129;     kvWPgVocFK61970129 = kvWPgVocFK9118515;     kvWPgVocFK9118515 = kvWPgVocFK9658494;     kvWPgVocFK9658494 = kvWPgVocFK48493324;     kvWPgVocFK48493324 = kvWPgVocFK9078269;     kvWPgVocFK9078269 = kvWPgVocFK60907398;     kvWPgVocFK60907398 = kvWPgVocFK99804137;     kvWPgVocFK99804137 = kvWPgVocFK83586106;     kvWPgVocFK83586106 = kvWPgVocFK25762124;     kvWPgVocFK25762124 = kvWPgVocFK27396948;     kvWPgVocFK27396948 = kvWPgVocFK53488671;     kvWPgVocFK53488671 = kvWPgVocFK85648352;     kvWPgVocFK85648352 = kvWPgVocFK60061954;     kvWPgVocFK60061954 = kvWPgVocFK13774104;     kvWPgVocFK13774104 = kvWPgVocFK46322798;     kvWPgVocFK46322798 = kvWPgVocFK77626076;     kvWPgVocFK77626076 = kvWPgVocFK66598504;     kvWPgVocFK66598504 = kvWPgVocFK44858084;     kvWPgVocFK44858084 = kvWPgVocFK38447112;     kvWPgVocFK38447112 = kvWPgVocFK40658859;     kvWPgVocFK40658859 = kvWPgVocFK18342127;     kvWPgVocFK18342127 = kvWPgVocFK48726441;     kvWPgVocFK48726441 = kvWPgVocFK1847232;     kvWPgVocFK1847232 = kvWPgVocFK73333287;     kvWPgVocFK73333287 = kvWPgVocFK23126068;     kvWPgVocFK23126068 = kvWPgVocFK49814290;     kvWPgVocFK49814290 = kvWPgVocFK9507675;     kvWPgVocFK9507675 = kvWPgVocFK77900822;     kvWPgVocFK77900822 = kvWPgVocFK96647546;     kvWPgVocFK96647546 = kvWPgVocFK42816942;     kvWPgVocFK42816942 = kvWPgVocFK43784758;     kvWPgVocFK43784758 = kvWPgVocFK85075483;     kvWPgVocFK85075483 = kvWPgVocFK50626242;     kvWPgVocFK50626242 = kvWPgVocFK40418754;     kvWPgVocFK40418754 = kvWPgVocFK86114587;     kvWPgVocFK86114587 = kvWPgVocFK81433608;     kvWPgVocFK81433608 = kvWPgVocFK22655592;     kvWPgVocFK22655592 = kvWPgVocFK26438120;     kvWPgVocFK26438120 = kvWPgVocFK7698995;     kvWPgVocFK7698995 = kvWPgVocFK34358058;     kvWPgVocFK34358058 = kvWPgVocFK38970102;}
// Junk Finished
