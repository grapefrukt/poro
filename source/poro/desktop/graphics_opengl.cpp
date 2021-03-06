/***************************************************************************
 *
 * Copyright (c) 2010 Petri Purho, Dennis Belfrage
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ***************************************************************************/

#include "graphics_opengl.h"

#include <cmath>

#include "../iplatform.h"
#include "../libraries.h"
#include "../poro_macros.h"
#include "texture_opengl.h"


#include "../external/stb_image.h"

#ifndef PORO_DONT_USE_GLEW
#	include "graphics_buffer_opengl.h"
#endif

#define PORO_ERROR "ERROR: "

#ifdef PORO_SAVE_ALPHA_FIXED_PNG_FILES
#	define STB_IMAGE_WRITE_IMPLEMENTATION
#	include "../external/stb_image_write.h"
#endif

//=============================================================================
namespace poro {

namespace {

	GraphicsSettings OPENGL_SETTINGS;

	Uint32 GetGLVertexMode(int vertex_mode){
		switch (vertex_mode) {
			case IGraphics::VERTEX_MODE_TRIANGLE_FAN:
				return GL_TRIANGLE_FAN;
			case IGraphics::VERTEX_MODE_TRIANGLE_STRIP:
				return GL_TRIANGLE_STRIP;
			default:
				poro_assert(false);
				break;
		}

		// as a default
		return GL_TRIANGLE_FAN;
	}

	types::vec2 Vec2Rotate( const types::vec2& point, const types::vec2& center, float angle )
	{
		types::vec2 D;
		D.x = point.x - center.x;
		D.y = point.y - center.y;

		// D.Rotate( angle );
		float tx = D.x;
		D.x = (float)D.x * (float)cos(angle) - D.y * (float)sin(angle);
		D.y = (float)tx * (float)sin(angle) + D.y * (float)cos(angle);

		// D += centre;
		D.x += center.x;
		D.y += center.y;

		return D;
	}

	//-------------------------------------------------------------------------

	struct Vertex
	{
		float x;
		float y;
		float tx;
		float ty;
	};

	//-------------------------------------------------------------------------

	void drawsprite( TextureOpenGL* texture, Vertex* vertices, const types::fcolor& color, int count, Uint32 vertex_mode, int blend_mode )
	{
		Uint32 tex = texture->mTexture;
		glBindTexture(GL_TEXTURE_2D, tex);
		glEnable(GL_TEXTURE_2D);
		// glEnable(GL_BLEND);

		glEnable(GL_BLEND);
		if( blend_mode == 0 ) {
			glColor4f(color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ] );
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		} else if( blend_mode == 1 ) {
			if( color[ 3 ] == 0 ) 
				return;

			glColor4f(
				color[ 0 ] / color[ 3 ], 
				color[ 1 ] / color[ 3 ], 
				color[ 2 ] / color[ 3 ], color[ 3 ] );

			glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		}

		glColor4f(color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ] );

		glBegin( vertex_mode );
		for( int i = 0; i < count; ++i )
		{
			glTexCoord2f(vertices[ i ].tx, vertices[ i ].ty );
			glVertex2f(vertices[ i ].x, vertices[ i ].y );
		}

		glEnd();
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

	//-------------------------------------------------------------------------

	void drawsprite_withalpha( TextureOpenGL* texture, Vertex* vertices, const types::fcolor& color, int count,
		TextureOpenGL* alpha_texture, Vertex* alpha_vertices, const types::fcolor& alpha_color,
		Uint32 vertex_mode )
	{
#ifdef PORO_DONT_USE_GLEW
		poro_logger << "Error: Glew isn't enable alpha masking, this means we can't do alpha masking. " << std::endl;
		return;
#else
		// no glew on mac? We'll maybe we need graphics_mac!?
		if(!GLEW_VERSION_1_3)
		{
			poro_logger << "Error: OpenGL 1.3. isn't supported, this means we can't do alpha masking. " << std::endl;
			return;
		}

		Uint32 image_id = texture->mTexture;
		Uint32 alpha_mask_id = alpha_texture->mTexture;

		// alpha texture
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);
		glColor4f(alpha_color[ 0 ], alpha_color[ 1 ], alpha_color[ 2 ], alpha_color[ 3 ] );
		glBindTexture(GL_TEXTURE_2D, alpha_mask_id );
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// sprite texture
        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
		glColor4f(color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ] );
        glBindTexture(GL_TEXTURE_2D, image_id );

		glDisable(GL_CULL_FACE);
		glBegin( vertex_mode );
		for( int i = 0; i < count; ++i )
		{
			// glTexCoord2f(vertices[ i ].tx, vertices[ i ].ty );
			glMultiTexCoord2f(GL_TEXTURE0, alpha_vertices[ i ].tx, alpha_vertices[ i ].ty);
			glMultiTexCoord2f(GL_TEXTURE1, vertices[i].tx, vertices[i].ty);
			glVertex2f(vertices[ i ].x, vertices[ i ].y );
		}

		glEnd();


		glDisable(GL_TEXTURE_2D);
		glDisable( GL_BLEND );

		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_TEXTURE_2D);
#endif
	}


	//-------------------------------------------------------------------------

	Uint32 GetNextPowerOfTwo(Uint32 input)
	{
		 --input;
		 input |= input >> 16;
		 input |= input >> 8;
		 input |= input >> 4;
		 input |= input >> 2;
		 input |= input >> 1;
		 return input + 1;
	}

	///////////////////////////////////////////////////////////////////////////

	unsigned char* ResizeImage( unsigned char* pixels, int w, int h, int new_size_x, int new_size_y )
	{
		const int bpp = 4;

		unsigned char* result = new unsigned char[ ( bpp * new_size_x * new_size_y ) ];

		for( int y = 0; y < new_size_y; ++y )
		{
			for( int x = 0; x < new_size_x; ++x )
			{
				
				int pixel_pos = ( ( y * w ) + x ) * bpp;
				int new_pos = ( ( y * new_size_x ) + x ) * bpp;
				for( int i = 0; i < bpp; ++i )
				{
					unsigned char c = 0;
					if( x < w && y < h ) 
						c = pixels[ pixel_pos + i ];

					result[ new_pos + i ] = c;
				}
			}
		}
		return result;
	}
	
	///////////////////////////////////////////////////////////////////////////

	TextureOpenGL* CreateImage( unsigned char* pixels, int w, int h, int bpp )
	{
		Uint32 oTexture = 0;
		float uv[4];
		int real_size[2];
		
		int nw = w; 
		int nh = h; 

		uv[0]=0;
		uv[1]=0;
		uv[2]=1;
		uv[3]=1;
		real_size[0] = w;
		real_size[1] = h;
		bool resize_to_power_of_two = OPENGL_SETTINGS.textures_resize_to_power_of_two;

		bool release_new_pixels = false;
		unsigned char* new_pixels = pixels;
		
	
		// --- power of 2
		if( resize_to_power_of_two )
		{
			nw = GetNextPowerOfTwo(w);
			nh = GetNextPowerOfTwo(h);
			if( nw != w || nh != h )
			{
				
				new_pixels = ResizeImage( pixels, w, h, nw, nh );
				release_new_pixels = true;

				uv[0] = 0;						// Min X
				uv[1] = 0;						// Min Y
				uv[2] = ((GLfloat)w ) / nw;	// Max X
				uv[3] = ((GLfloat)h ) / nh;	// Max Y

				real_size[ 0 ] = nw;
				real_size[ 1 ] = nh;
			}
		}
		// --- /power of 2 

		glGenTextures(1, (GLuint*)&oTexture);
		glBindTexture(GL_TEXTURE_2D, oTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nw, nh, 0,
			 GL_RGBA, GL_UNSIGNED_BYTE, new_pixels);
	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


		if( release_new_pixels )
			delete [] new_pixels;

		TextureOpenGL* result = new TextureOpenGL;
		result->mTexture = oTexture;
		result->mWidth = w;
		result->mHeight = h;
		result->mRealSizeX = real_size[ 0 ];
		result->mRealSizeY = real_size[ 1 ];

		for( int i = 0; i < 4; ++i )
			result->mUv[ i ] = uv[ i ];
		return result;
	}
			

	//-----------------------------------------------------------------------------

	// Thanks to Jetro Lauha for allowing me to use this. 
	// ripped from: http://code.google.com/p/turska/ 
	// T2GraphicsOpenGL.cpp
	// 
	void GetSimpleFixAlphaChannel( unsigned char* pixels, int w, int h, int bpp )
	{

		using namespace types;
		if( w < 2 || h < 2)
			return;
		
		types::Int32 x, y;
		types::Int32 co = 0;

		for (y = 0; y < h; ++y)
		{
			for (x = 0; x < w; ++x)
			{
				co = ( ( y * w ) + x ) * bpp;

				if ((pixels[co + 3]) == 0)
				{
					// iterate through 3x3 window around pixel
					types::Int32 left = x - 1, right = x + 1, top = y - 1, bottom = y + 1;
					if( left < 0 ) left = 0;
					if( right >= w ) right = w - 1;
					if( top < 0 ) top = 0;
					if( bottom >= h ) bottom = h - 1;
					types::Int32 x2, y2, colors = 0, co2 = top * w + left;
					types::Int32 red = 0, green = 0, blue = 0;
					for(y2 = top; y2 <= bottom; ++y2)
					{
						for(x2 = left; x2 <= right; ++x2)
						{
							co2 = ( ( y2 * w ) + x2 ) * bpp;
							
							if(pixels[co2 + 3])
							{
								red += pixels[co2 + 0];
								green += pixels[co2 + 1];
								blue += pixels[co2 + 2];
								++colors;
							}
						}
					}
					if( colors > 0)
					{
						pixels[co + 3 ] = 0;
						pixels[co + 0 ] = (red / colors);
						pixels[co + 1 ] = (green / colors);
						pixels[co + 2 ] = (blue / colors);
					}
				}
			}
		}
	}

	//-------------------------------------------------------------------------

	std::string GetFileExtension( const std::string& filename )
	{
		unsigned int p = filename.find_last_of( "." );
		if( p < ( filename.size() - 1 ) )
			return filename.substr( p + 1 );

		return "";
	}

	TextureOpenGL* LoadTextureForReal( const types::string& filename )
	{
		TextureOpenGL* result = NULL;
		
		int x,y,bpp;
		unsigned char *data = stbi_load(filename.c_str(), &x, &y, &bpp, 4);

		if( data == NULL ) 
			return NULL;
		
		// ... process data if not NULL ... 
		// ... x = width, y = height, n = # 8-bit components per pixel ...
		// ... replace '0' with '1'..'4' to force that many components per pixel
		
		if( OPENGL_SETTINGS.textures_fix_alpha_channel && bpp == 4 ) 
		{
			GetSimpleFixAlphaChannel( data, x, y, bpp );
			
#ifdef PORO_SAVE_ALPHA_FIXED_PNG_FILES
			if( false && GetFileExtension( filename ) == "png" )
			{
				int result = stbi_write_png( filename.c_str(), x, y, 4, data, x * 4 );
				if( result == 0 ) std::cout << "problems saving: " << filename << std::endl;
			}
#endif
		}

		result = CreateImage( data, x, y, bpp );
		stbi_image_free(data);

		return result;
	}

	//-----------------------------------------------------------------------------

	TextureOpenGL* CreateTextureForReal(int width,int height)
	{
		TextureOpenGL* result = NULL;
		
		int bpp = 4;
		int char_size = width * height * bpp;
		unsigned char* data = new unsigned char[ char_size ];

		for( int i = 0; i < char_size; ++i )
		{
			data[ i ] = 0;
		}

		if( data )
			result = CreateImage( data, width, height, bpp );

		delete [] data;
		data = NULL;

		return result;
	}

	void SetTextureDataForReal(TextureOpenGL* texture, void* data)
	{
		// update the texture image:
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, (GLuint)texture->mTexture);
 		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->mWidth, texture->mHeight, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glDisable(GL_TEXTURE_2D);
	}

} // end o namespace anon

void GraphicsOpenGL::SetSettings( const GraphicsSettings& settings )
{
	OPENGL_SETTINGS = settings;
}


bool GraphicsOpenGL::Init( int width, int height, bool fullscreen, const types::string& caption )
{
    mFullscreen = fullscreen;
    mWindowWidth = width;
    mWindowHeight = height;
    mDesktopWidth = 0;
	mDesktopHeight = 0;
    mGlContextInitialized = false;

	const SDL_VideoInfo *info = NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
		poro_logger << PORO_ERROR << "Video initialization failed:  " << SDL_GetError() << std::endl;
        SDL_Quit();
        exit(0);
    }

    info = SDL_GetVideoInfo();
	if (!info)
    {
		poro_logger << PORO_ERROR << "Video query failed: "<< SDL_GetError() << std::endl;
        SDL_Quit();
        exit(0);
    }
    mDesktopWidth = (float)info->current_w;
	mDesktopHeight = (float)info->current_h;
	
	IPlatform::Instance()->SetInternalSize( (types::Float32)width, (types::Float32)height );
    ResetWindow();

    SDL_WM_SetCaption( caption.c_str(), NULL);
	
	// no glew for mac? this might cause some problems
#ifndef PORO_DONT_USE_GLEW
	GLenum glew_err = glewInit();
	if (GLEW_OK != glew_err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		poro_logger << "Error: " << glewGetErrorString(glew_err) << std::endl;
	}
#endif
	return 1;
}

void GraphicsOpenGL::SetInternalSize( types::Float32 width, types::Float32 height )
{
    if( mGlContextInitialized )
	{
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        gluOrtho2D(0, (GLdouble)width, (GLdouble)height, 0);
    }
}

void GraphicsOpenGL::SetWindowSize(int window_width, int window_height)
{
    if( mWindowWidth != window_width || mWindowHeight != window_height )
	{
        mWindowWidth = window_width;
        mWindowHeight = window_height;
        ResetWindow();
    }
}

void GraphicsOpenGL::SetFullscreen(bool fullscreen)
{
    if( mFullscreen!=fullscreen )
	{
        mFullscreen = fullscreen;
        ResetWindow();
    }
}

void GraphicsOpenGL::ResetWindow()
{
	const SDL_VideoInfo *info = NULL;
    int bpp = 0;
    int flags = 0;
    int window_width;
	int window_height;
	
    info = SDL_GetVideoInfo();
	if (!info)
    {
		poro_logger << PORO_ERROR << "Video query failed: "<< SDL_GetError() << std::endl;
        SDL_Quit();
        exit(0);
    }
    
    {
        bpp = info->vfmt->BitsPerPixel;
        flags = SDL_OPENGL;
        
		if( mFullscreen ){
			flags |= SDL_FULLSCREEN;
			window_width = (int)mDesktopWidth;
			window_height = (int)mDesktopHeight;
    	} else {
    		window_width = mWindowWidth;
			window_height = mWindowHeight;
    	    
		#ifdef _DEBUG
            flags |= SDL_RESIZABLE;
        #endif
    	}
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (SDL_SetVideoMode((int)window_width, (int)window_height, bpp, flags) == 0)
    {
        fprintf( stderr, "Video mode set failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }
    mGlContextInitialized = true;
    
    
    { //OpenGL view setup
        float internal_width = IPlatform::Instance()->GetInternalWidth();
        float internal_height = IPlatform::Instance()->GetInternalHeight();
        float screen_aspect = (float)window_width/(float)window_height;
        float internal_aspect = (float)internal_width/(float)internal_height;
        mViewportSize.x = (float)window_width;
        mViewportSize.y = (float)window_height;
        mViewportOffset = types::vec2(0, 0);
        if(screen_aspect>internal_aspect){
            //Widescreen, Black borders on left and right
            mViewportSize.x = window_height*internal_aspect;
            mViewportOffset.x = (window_width-mViewportSize.x)*0.5f;
        } else {
            //Tallscreen, Black borders on top and bottom
            mViewportSize.y = window_width/internal_aspect;
            mViewportOffset.y = (window_height-mViewportSize.y)*0.5f;
        }
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

		glViewport((GLint)mViewportOffset.x, (GLint)mViewportOffset.y, (GLint)mViewportSize.x, (GLint)mViewportSize.y);
		
		glClearColor(0,0,0,1.0f);
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        
		//(OpenGL actually wants the x offset from the bottom, but since we are centering the view the direction does not matter.)
		// glEnable(GL_SCISSOR_TEST);
	    // glScissor((GLint)mViewportOffset.x, (GLint)mViewportOffset.y, (GLint)mViewportSize.x, (GLint)mViewportSize.y);

		glScalef(1,-1,1); //Flip y axis
        gluOrtho2D(0, internal_width, 0, internal_height);
    }
}


//=============================================================================

ITexture* GraphicsOpenGL::CreateTexture( int width, int height )
{
	return CreateTextureForReal(width,height);
}

ITexture* GraphicsOpenGL::CloneTexture( ITexture* other )
{
	return new TextureOpenGL( dynamic_cast< TextureOpenGL* >( other ) );
}

void GraphicsOpenGL::SetTextureData( ITexture* texture, void* data )
{
	SetTextureDataForReal( dynamic_cast< TextureOpenGL* >( texture ), data );
}

ITexture* GraphicsOpenGL::LoadTexture( const types::string& filename )
{
	ITexture* result = LoadTextureForReal( filename );
	
	if( result == NULL )
		poro_logger << "Couldn't load image: " << filename << std::endl;
		
	TextureOpenGL* texture = dynamic_cast< TextureOpenGL* >( result );
	if( texture )
		texture->SetFilename( filename );

	return result;
}

void GraphicsOpenGL::ReleaseTexture( ITexture* itexture )
{
	TextureOpenGL* texture = dynamic_cast< TextureOpenGL* >( itexture );
	poro_assert( texture );
	glDeleteTextures(1, &texture->mTexture);
}
//=============================================================================

void GraphicsOpenGL::DrawTexture( ITexture* itexture, float x, float y, float w, float h, const types::fcolor& color, float rotation )
{
	if( itexture == NULL )
		return;

	if( color[3] <= 0 )
		return;

	TextureOpenGL* texture = (TextureOpenGL*)itexture;

	static types::vec2 temp_verts[ 4 ];
	static types::vec2 tex_coords[ 4 ];

	temp_verts[ 0 ].x = (float)x;
	temp_verts[ 0 ].y = (float)y;
	temp_verts[ 1 ].x = (float)x;
	temp_verts[ 1 ].y = (float)(y + h);
	temp_verts[ 2 ].x = (float)(x + w);
	temp_verts[ 2 ].y = (float)y;
	temp_verts[ 3 ].x = (float)(x + w);
	temp_verts[ 3 ].y = (float)(y + h);

	if( rotation != 0 )
	{
		types::vec2 center_p;
		center_p.x = temp_verts[ 0 ].x + ( ( temp_verts[ 3 ].x - temp_verts[ 0 ].x ) * 0.5f );
		center_p.y = temp_verts[ 0 ].y + ( ( temp_verts[ 3 ].y - temp_verts[ 0 ].y ) * 0.5f );

		for( int i = 0; i < 4; ++i )
		{
			temp_verts[ i ] = Vec2Rotate( temp_verts[ i ], center_p, rotation );
		}
	}

	float tx1 = 0;
	float ty1 = 0;
	float tx2 = (float)texture->GetWidth();
	float ty2 = (float)texture->GetHeight();

	tex_coords[ 0 ].x = tx1;
	tex_coords[ 0 ].y = ty1;

	tex_coords[ 1 ].x = tx1;
	tex_coords[ 1 ].y = ty2;

	tex_coords[ 2 ].x = tx2;
	tex_coords[ 2 ].y = ty1;

	tex_coords[ 3 ].x = tx2;
	tex_coords[ 3 ].y = ty2;

	PushVertexMode(poro::IGraphics::VERTEX_MODE_TRIANGLE_STRIP);
	DrawTexture( texture,  temp_verts, tex_coords, 4, color );
	PopVertexMode();

}
//=============================================================================

void GraphicsOpenGL::DrawTexture( ITexture* itexture, types::vec2* vertices, types::vec2* tex_coords, int count, const types::fcolor& color )
{
	poro_assert( count <= 8 );

	if( itexture == NULL )
		return;

	if( color[3] <= 0 )
		return;

	TextureOpenGL* texture = (TextureOpenGL*)itexture;

	for( int i = 0; i < count; ++i )
	{
		tex_coords[ i ].x *= texture->mExternalSizeX;
		tex_coords[ i ].y *= texture->mExternalSizeY;
	}


	static Vertex vert[8];

	float x_text_conv = ( 1.f / texture->mWidth ) * ( texture->mUv[ 2 ] - texture->mUv[ 0 ] );
	float y_text_conv = ( 1.f / texture->mHeight ) * ( texture->mUv[ 3 ] - texture->mUv[ 1 ] );
	for( int i = 0; i < count; ++i )
	{
		vert[i].x = vertices[i].x;
		vert[i].y = vertices[i].y;
		vert[i].tx = texture->mUv[ 0 ] + ( tex_coords[i].x * x_text_conv );
		vert[i].ty = texture->mUv[ 1 ] + ( tex_coords[i].y * y_text_conv );
	}

	drawsprite( texture, vert, color, count, GetGLVertexMode(mVertexMode), mBlendMode );
}

//-----------------------------------------------------------------------------

void GraphicsOpenGL::DrawTextureWithAlpha(
		ITexture* itexture, types::vec2* vertices, types::vec2* tex_coords, int count, const types::fcolor& color,
		ITexture* ialpha_texture, types::vec2* alpha_vertices, types::vec2* alpha_tex_coords, const types::fcolor& alpha_color )
{
	if( itexture == NULL || ialpha_texture == NULL )
		return;

	if( color[3] <= 0 || alpha_color[3] <= 0 )
		return;

	TextureOpenGL* texture = (TextureOpenGL*)itexture;
	TextureOpenGL* alpha_texture = (TextureOpenGL*)ialpha_texture;

	for( int i = 0; i < 4; ++i )
	{
		tex_coords[ i ].x *= texture->mExternalSizeX;
		tex_coords[ i ].y *= texture->mExternalSizeY;
		alpha_tex_coords[ i ].x *= alpha_texture->mExternalSizeX;
		alpha_tex_coords[ i ].y *= alpha_texture->mExternalSizeY;
	}


	static Vertex vert[8];
	static Vertex alpha_vert[8];

	poro_assert( count > 2 );
	poro_assert( count <= 8 );

	// vertices
	float x_text_conv = ( 1.f / texture->mWidth ) * ( texture->mUv[ 2 ] - texture->mUv[ 0 ] );
	float y_text_conv = ( 1.f / texture->mHeight ) * ( texture->mUv[ 3 ] - texture->mUv[ 1 ] );
	float x_alpha_text_conv = ( 1.f / alpha_texture->mWidth ) * ( alpha_texture->mUv[ 2 ] - alpha_texture->mUv[ 0 ] );
	float y_alpha_text_conv = ( 1.f / alpha_texture->mHeight ) * ( alpha_texture->mUv[ 3 ] - alpha_texture->mUv[ 1 ] );

	for( int i = 0; i < count; ++i )
	{
		vert[i].x = vertices[ i ].x;
		vert[i].y = vertices[ i ].y;
		vert[i].tx = texture->mUv[ 0 ] + ( tex_coords[ i ].x * x_text_conv );
		vert[i].ty = texture->mUv[ 1 ] + ( tex_coords[ i ].y * y_text_conv );

		alpha_vert[i].x = alpha_vertices[i].x;
		alpha_vert[i].y = alpha_vertices[i].y;
		alpha_vert[i].tx = alpha_texture->mUv[ 0 ] + ( alpha_tex_coords[ i ].x * x_alpha_text_conv );
		alpha_vert[i].ty = alpha_texture->mUv[ 1 ] + ( alpha_tex_coords[ i ].y * y_alpha_text_conv );
	}

	drawsprite_withalpha( texture, vert, color, count,
		alpha_texture, alpha_vert, alpha_color,
		GetGLVertexMode(mVertexMode) );
}

//=============================================================================

void GraphicsOpenGL::BeginRendering()
{
    if( mClearBackground){
        glClearColor( mFillColor[ 0 ],
            mFillColor[ 1 ],
            mFillColor[ 2 ],
            mFillColor[ 3 ] );

        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    }
}

void GraphicsOpenGL::EndRendering()
{
	SDL_GL_SwapBuffers();
}

//=============================================================================

void GraphicsOpenGL::DrawLines( const std::vector< poro::types::vec2 >& vertices, const types::fcolor& color, bool smooth, float width, bool loop )
{
	//float xPlatformScale, yPlatformScale;
	//xPlatformScale = (float)mViewportSize.x / (float)poro::IPlatform::Instance()->GetInternalWidth();
	//yPlatformScale = (float)mViewportSize.y / (float)poro::IPlatform::Instance()->GetInternalHeight();
	
	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth( width );

	if( smooth ) {
		glEnable(GL_LINE_SMOOTH);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); 
	}
	glColor4f( color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ] );
	glBegin(loop?GL_LINE_LOOP:GL_LINE_STRIP);

	for( std::size_t i = 0; i < vertices.size(); ++i )
	{
		glVertex2f(vertices[i].x, vertices[i].y);
	}
	glEnd();

	if( smooth ) 
		glDisable( GL_LINE_SMOOTH );

	glDisable(GL_BLEND);
}

//-----------------------------------------------------------------------------

void GraphicsOpenGL::DrawFill( const std::vector< poro::types::vec2 >& vertices, const types::fcolor& color )
{
	int vertCount = vertices.size();
	
	if(vertCount == 0)
		return;
	
	//Internal rescale
	float xPlatformScale, yPlatformScale;
	xPlatformScale = (float)mViewportSize.x / (float)poro::IPlatform::Instance()->GetInternalWidth();
	yPlatformScale = (float)mViewportSize.y / (float)poro::IPlatform::Instance()->GetInternalHeight();

	xPlatformScale = 1.f;
	yPlatformScale = 1.f;

	
	const int max_buffer_size = 256;
	static GLfloat glVertices[ max_buffer_size ];

	poro_assert( vertCount * 2 <= max_buffer_size );

	int o = -1;
	for(int i=0; i < vertCount; ++i){
		glVertices[++o] = vertices[i].x*xPlatformScale;
		glVertices[++o] = vertices[i].y*yPlatformScale;
	}

	glEnable(GL_BLEND);
	glColor4f(color[0], color[1], color[2], color[3]);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();
		glVertexPointer(2, GL_FLOAT , 0, glVertices);
		glEnableClientState(GL_VERTEX_ARRAY);
		// glDrawArrays(GL_TRIANGLE_STRIP, 0, vertCount);
		glDrawArrays(GL_TRIANGLE_FAN, 0, vertCount);
		glDisableClientState(GL_VERTEX_ARRAY);
	glPopMatrix();

	glDisable(GL_BLEND);
}

void GraphicsOpenGL::DrawTexturedRect( const poro::types::vec2& position, const poro::types::vec2& size, ITexture* itexture )
{
	if( itexture == NULL )
		return;

	TextureOpenGL* texture = (TextureOpenGL*)itexture;

	static types::vec2 vertices[ 4 ];
	vertices[ 0 ].x = (float) position.x;
	vertices[ 0 ].y = (float) position.y;
	vertices[ 1 ].x = (float) position.x;
	vertices[ 1 ].y = (float) (position.y + size.y);
	vertices[ 2 ].x = (float) (position.x + size.x);
	vertices[ 2 ].y = (float) position.y;
	vertices[ 3 ].x = (float) (position.x + size.x);
	vertices[ 3 ].y = (float) (position.y + size.y);

	Uint32 tex = texture->mTexture;

	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin( GL_TRIANGLE_STRIP );

	for( int i = 0; i < 4; i++)
	{
		glTexCoord2f(vertices[ i ].x / texture->GetWidth(), vertices[ i ].y / texture->GetHeight() );
		glVertex2f(vertices[ i ].x, vertices[ i ].y );
	}
	
	glEnd();
	glDisable(GL_BLEND);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDisable(GL_TEXTURE_2D);
}

//=============================================================================

types::vec2	GraphicsOpenGL::ConvertToInternalPos( int x, int y ) 
{
	types::vec2 result( (types::Float32)x, (types::Float32)y );

	result.x -= mViewportOffset.x;
	result.y -= mViewportOffset.y;
	
	//Clamp
    if(result.x<0)
        result.x=0;
    if(result.y<0)
        result.y=0;
    if(result.x>mViewportSize.x-1)
        result.x=mViewportSize.x-1;
    if(result.y>mViewportSize.y-1)
        result.y=mViewportSize.y-1;

    float internal_w = IPlatform::Instance()->GetInternalWidth();
    float internal_h = IPlatform::Instance()->GetInternalHeight();
    
	result.x *= internal_w / (types::Float32)mViewportSize.x;
	result.y *= internal_h / (types::Float32)mViewportSize.y;
    
	return result;
}

//=============================================================================

IGraphicsBuffer* GraphicsOpenGL::CreateGraphicsBuffer(int width, int height)
{
#ifdef PORO_DONT_USE_GLEW
	poro_assert(false); //Buffer implementation needs glew.
	return NULL;
#else
	GraphicsBufferOpenGL* buffer = new GraphicsBufferOpenGL;
	buffer->Init(width, height);
	return buffer;
#endif
}

void GraphicsOpenGL::DestroyGraphicsBuffer(IGraphicsBuffer* buffer)
{
#ifdef PORO_DONT_USE_GLEW
	poro_assert(false); //Buffer implementation needs glew.
#else
	delete buffer;
#endif
}

//=============================================================================

} // end o namespace poro
