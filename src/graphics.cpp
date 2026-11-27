#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <string>

#include "graphics.h"
#include "log.h"

Scene2D::Scene2D(int w, int h, int pixelDepth)
{
#ifdef GRAPHICS_USES_FONT
	this->fontReady = false;
	this->ftLib = NULL;
#endif
	this->width = w;
	this->height = h;
	this->depth = pixelDepth;
	
	this->frameBufferSize = this->width * this->height * this->depth;
}

bool Scene2D::Init(size_t memSize, int numFrameBuffers)
{
	int rc;
	
	this->video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
	this->videoMem = NULL;

	if (this->video < 0)
	{
		DEBUGLOG << "Failed to open a video out handle: " << std::string(strerror(errno));
		return false;
	}
	
#ifdef GRAPHICS_USES_FONT
	// FreeType improves text rendering, but video output must still start if
	// the optional system module is missing on a particular firmware.
	rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
	if (rc < 0)
	{
		DEBUGLOG << "FreeType system module unavailable: " << std::string(strerror(errno));
	}
	else
	{
		rc = FT_Init_FreeType(&this->ftLib);
		if (rc != 0)
			DEBUGLOG << "FreeType initialization failed: " << std::string(strerror(errno));
		else
			this->fontReady = true;
	}
#endif
	
	if(!initFlipQueue())
	{
		DEBUGLOG << "Failed to initialize flip queue: " << std::string(strerror(errno));
		return false;
	}
	
	if(!allocateVideoMem(memSize, 0x200000))
	{
		DEBUGLOG << "Failed to allocate video memory: " << std::string(strerror(errno));
		return false;
	}
	
	if(!allocateFrameBuffers(numFrameBuffers))
	{
		DEBUGLOG << "Failed to allocate frame buffers: " << std::string(strerror(errno));
		return false;
	}
	
	sceVideoOutSetFlipRate(this->video, 0);
	return true;
}

bool Scene2D::initFlipQueue()
{
	int rc = sceKernelCreateEqueue(&flipQueue, "homebrew flip queue");
	
	if(rc < 0)
		return false;
		
	sceVideoOutAddFlipEvent(flipQueue, this->video, 0);
	return true;
}

bool Scene2D::allocateFrameBuffers(int num)
{
	// Allocate frame buffers array
	this->frameBuffers = new char*[num];
	
	// Set the display buffers
	for(int i = 0; i < num; i++)
		this->frameBuffers[i] = this->allocateDisplayMem(frameBufferSize);

	// Set SRGB pixel format
	sceVideoOutSetBufferAttribute(&this->attr, 0x80000000, 1, 0, this->width, this->height, this->width);
	
	// Register the buffers to the video handle
	return (sceVideoOutRegisterBuffers(this->video, 0, (void **)this->frameBuffers, num, &this->attr) == 0);
}

char *Scene2D::allocateDisplayMem(size_t size)
{
	// Essentially just bump allocation
	char *allocatedPtr = (char *)videoMemSP;
	videoMemSP += size;

	return allocatedPtr;
}

bool Scene2D::allocateVideoMem(size_t size, int alignment)
{
	int rc;
	
	// Align the allocation size
	this->directMemAllocationSize = (size + alignment - 1) / alignment * alignment;
	
	// Allocate memory for display buffer
	rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), this->directMemAllocationSize, alignment, 3, &this->directMemOff);
	
	if(rc < 0)
	{
		this->directMemAllocationSize = 0;
		return false;
	}
	
	// Map the direct memory
	rc = sceKernelMapDirectMemory(&this->videoMem, this->directMemAllocationSize, 0x33, 0, this->directMemOff, alignment);
	
	if(rc < 0)
	{
		sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
		
		this->directMemOff = 0;
		this->directMemAllocationSize = 0;
		
		return false;
	}
	
	// Set the stack pointer to the beginning of the buffer
	this->videoMemSP = (uintptr_t)this->videoMem;
	return true;
}

void Scene2D::deallocateVideoMem()
{
	// Free the direct memory
	sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
	
	// Zero out meta data
	this->videoMem = 0;
	this->videoMemSP = 0;
	this->directMemOff = 0;
	this->directMemAllocationSize = 0;
	
	// Free the frame buffer array
	delete this->frameBuffers;
	this->frameBuffers = 0;
}

void Scene2D::SetActiveFrameBuffer(int index)
{
	this->activeFrameBufferIdx = index;
}

void Scene2D::SubmitFlip(int frameID)
{
	sceVideoOutSubmitFlip(this->video, this->activeFrameBufferIdx, ORBIS_VIDEO_OUT_FLIP_VSYNC, frameID);
}

void Scene2D::FrameWait(int frameID)
{
	OrbisKernelEvent evt;
	int count;
	
	// If the video handle is not initialized, bail out. This is mostly a failsafe, this should never happen.
	if(this->video == 0)
		return;
		
	for(;;)
	{
		OrbisVideoOutFlipStatus flipStatus;
		
		// Get the flip status and check the arg for the given frame ID
		sceVideoOutGetFlipStatus(video, &flipStatus);
		
		if(flipStatus.flipArg == frameID)
			break;
			
		// Wait on next flip event
		if(sceKernelWaitEqueue(this->flipQueue, &evt, 1, &count, 0) != 0)
			break;
	}
}

void Scene2D::FrameBufferSwap()
{
	// Swap the frame buffer for some perf
	this->activeFrameBufferIdx = (this->activeFrameBufferIdx + 1) % 2;
}

void Scene2D::FrameBufferClear()
{
	// Clear the screen with a white frame buffer
	Color blank = { 255, 255, 255 };
	FrameBufferFill(blank);
}

#ifdef GRAPHICS_USES_FONT
bool Scene2D::InitFont(FT_Face *face, const char *fontPath, int fontSize)
{
	if (!this->fontReady || face == NULL)
		return false;

	const int rc = FT_New_Face(this->ftLib, fontPath, 0, face);
	if (rc != 0)
		return false;

	return FT_Set_Pixel_Sizes(*face, 0, fontSize) == 0;
}
#endif

void Scene2D::FrameBufferFill(Color color)
{
	DrawRectangle(0, 0, this->width, this->height, color);
}

void Scene2D::DrawPixel(int x, int y, Color color)
{
	// Get pixel location based on pitch
	int pixel = (y * this->width) + x;
	
	// Encode to 24-bit color
	uint32_t encodedColor = 0x80000000 + (color.r << 16) + (color.g << 8) + color.b;
	
	// Draw to the frame buffer
	((uint32_t *)this->frameBuffers[this->activeFrameBufferIdx])[pixel] = encodedColor;
}

void Scene2D::DrawRectangle(int x, int y, int w, int h, Color color)
{
	int xPos, yPos;
	
	// Draw row-by-row, column-by-column
	for(yPos = y; yPos < y + h; yPos++)
	{
		for(xPos = x; xPos < x + w; xPos++)
		{
			DrawPixel(xPos, yPos, color);
		}
	}
}

#ifdef GRAPHICS_USES_FONT
void Scene2D::DrawText(char *txt, FT_Face face, int startX, int startY, Color bgColor, Color fgColor)
{
	if (!this->fontReady || txt == NULL || face == NULL)
		return;

	int xOffset = 0;
	int yOffset = 0;
	const int textLength = strlen(txt);
	FT_GlyphSlot slot = face->glyph;

	for (int n = 0; n < textLength; ++n)
	{
		const unsigned char character = static_cast<unsigned char>(txt[n]);
		if (character == '\n')
		{
			xOffset = 0;
			yOffset += static_cast<int>(face->size->metrics.height >> 6);
			continue;
		}

		const FT_UInt glyphIndex = FT_Get_Char_Index(face, character);
		if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0 ||
			FT_Render_Glyph(slot, ft_render_mode_normal) != 0)
			continue;

		const int pitch = slot->bitmap.pitch;
		for (int yPos = 0; yPos < slot->bitmap.rows; ++yPos)
		{
			const unsigned char *row = pitch >= 0
				? slot->bitmap.buffer + yPos * pitch
				: slot->bitmap.buffer + (slot->bitmap.rows - 1 - yPos) * -pitch;
			for (int xPos = 0; xPos < slot->bitmap.width; ++xPos)
			{
				const uint8_t alpha = row[xPos];
				if (alpha == 0)
					continue;

				const int x = startX + xOffset + xPos + slot->bitmap_left;
				const int y = startY + yOffset + yPos - slot->bitmap_top;
				if (x < 0 || y < 0 || x >= this->width || y >= this->height)
					continue;

				// Blend against the pixel already on screen so edges stay smooth
				// on any surface, including gradients and highlighted cards.
				const int pixel = y * this->width + x;
				const uint32_t background =
					reinterpret_cast<uint32_t *>(this->frameBuffers[this->activeFrameBufferIdx])[pixel];
				Color under = bgColor;
				if (background & 0x80000000)
			{
					under.r = static_cast<uint8_t>((background >> 16) & 0xff);
					under.g = static_cast<uint8_t>((background >> 8) & 0xff);
					under.b = static_cast<uint8_t>(background & 0xff);
				}
				const uint8_t inverse = 255 - alpha;
				const Color finalColor = {
					static_cast<uint8_t>((alpha * fgColor.r + inverse * under.r + 127) / 255),
					static_cast<uint8_t>((alpha * fgColor.g + inverse * under.g + 127) / 255),
					static_cast<uint8_t>((alpha * fgColor.b + inverse * under.b + 127) / 255)
				};
				this->DrawPixel(x, y, finalColor);
			}
		}
		xOffset += slot->advance.x >> 6;
	}
}
#endif




