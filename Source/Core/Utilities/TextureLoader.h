#pragma once

#include "Types.h"
#include <memory>

// Simple BMP file header structures
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16 bfType;      // File type, should be 'BM'
    uint32 bfSize;      // Size of file in bytes
    uint16 bfReserved1; // Reserved
    uint16 bfReserved2; // Reserved
    uint32 bfOffBits;   // Offset to bitmap data
};

struct BMPInfoHeader {
    uint32 biSize;          // Size of this header
    int32  biWidth;         // Image width
    int32  biHeight;        // Image height
    uint16 biPlanes;        // Number of planes
    uint16 biBitCount;      // Bits per pixel
    uint32 biCompression;   // Compression type
    uint32 biSizeImage;     // Size of image data
    int32  biXPelsPerMeter; // X pixels per meter
    int32  biYPelsPerMeter; // Y pixels per meter
    uint32 biClrUsed;       // Colors used
    uint32 biClrImportant;  // Important colors
};
#pragma pack(pop)

// Simple texture image data structure
struct TextureImageData {
    uint32 width = 0;
    uint32 height = 0;
    uint32 channels = 4; // Always convert to RGBA
    std::unique_ptr<uint8[]> pixels;
    
    bool IsValid() const { return pixels != nullptr && width > 0 && height > 0; }
    size_t GetDataSize() const { return width * height * channels; }
};

// Texture loader utility class
class TextureLoader {
public:
    // Load texture from file (supports .bmp and .dds)
    static TextureImageData LoadFromFile(const String& filePath);
    
    // Create test textures programmatically
    static TextureImageData CreateTestPattern(uint32 width, uint32 height, const String& pattern = "checkerboard");
    static TextureImageData CreateSolidColor(uint32 width, uint32 height, uint8 r, uint8 g, uint8 b, uint8 a = 255);
    static TextureImageData CreateGradient(uint32 width, uint32 height);
    static TextureImageData CreateUVTest(uint32 width, uint32 height);

private:
    // Format specific loading
    static TextureImageData LoadBMP(const String& filePath);
    static TextureImageData LoadDDS(const String& filePath);
    
    // Helper functions
    static bool ValidateBMPHeaders(const BMPFileHeader& fileHeader, const BMPInfoHeader& infoHeader);
    static void ConvertToRGBA(const uint8* srcData, uint8* dstData, uint32 width, uint32 height, uint16 bitCount);
    static void FlipImageVertically(uint8* data, uint32 width, uint32 height, uint32 channels);
};