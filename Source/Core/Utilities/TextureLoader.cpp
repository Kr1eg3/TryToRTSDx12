#include "TextureLoader.h"
#include "../../Platform/Windows/WindowsPlatform.h"
#include <fstream>
#include <algorithm>

TextureImageData TextureLoader::LoadFromFile(const String& filePath) {
    // Determine file type by extension
    if (filePath.ends_with(".bmp") || filePath.ends_with(".BMP")) {
        return LoadBMP(filePath);
    } else if (filePath.ends_with(".dds") || filePath.ends_with(".DDS")) {
        return LoadDDS(filePath);
    }
    
    Platform::OutputDebugMessage("TextureLoader: Unsupported file format: " + filePath + "\n");
    
    // Return fallback checkerboard texture
    Platform::OutputDebugMessage("TextureLoader: Creating fallback checkerboard texture\n");
    return CreateTestPattern(64, 64, "checkerboard");
}

TextureImageData TextureLoader::LoadBMP(const String& filePath) {
    TextureImageData result;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        Platform::OutputDebugMessage("TextureLoader: Failed to open file: " + filePath + "\n");
        return result;
    }
    
    // Read file header
    BMPFileHeader fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(BMPFileHeader));
    
    // Read info header
    BMPInfoHeader infoHeader;
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(BMPInfoHeader));
    
    if (!ValidateBMPHeaders(fileHeader, infoHeader)) {
        Platform::OutputDebugMessage("TextureLoader: Invalid BMP file: " + filePath + "\n");
        return result;
    }
    
    // Set up result
    result.width = static_cast<uint32>(abs(infoHeader.biWidth));
    result.height = static_cast<uint32>(abs(infoHeader.biHeight));
    result.channels = 4; // Always convert to RGBA
    
    // Calculate source data size
    uint32 bytesPerPixel = infoHeader.biBitCount / 8;
    uint32 rowSize = ((infoHeader.biBitCount * result.width + 31) / 32) * 4; // BMP rows are padded to 4 bytes
    uint32 sourceDataSize = rowSize * result.height;
    
    // Read source data
    auto sourceData = std::make_unique<uint8[]>(sourceDataSize);
    file.seekg(fileHeader.bfOffBits, std::ios::beg);
    file.read(reinterpret_cast<char*>(sourceData.get()), sourceDataSize);
    
    // Allocate destination data
    result.pixels = std::make_unique<uint8[]>(result.GetDataSize());
    
    // Convert to RGBA
    ConvertToRGBA(sourceData.get(), result.pixels.get(), result.width, result.height, infoHeader.biBitCount);
    
    // BMP images are stored bottom-to-top, so flip vertically
    if (infoHeader.biHeight > 0) {
        FlipImageVertically(result.pixels.get(), result.width, result.height, result.channels);
    }
    
    Platform::OutputDebugMessage("TextureLoader: Successfully loaded BMP: " + filePath + 
                                " (" + std::to_string(result.width) + "x" + std::to_string(result.height) + ")\n");
    
    return result;
}

TextureImageData TextureLoader::LoadDDS(const String& filePath) {
    Platform::OutputDebugMessage("TextureLoader: DDS loading not fully implemented, creating fallback texture for: " + filePath + "\n");
    
    // For now, create a test texture based on the filename
    TextureImageData result;
    
    if (filePath.find("bricks") != String::npos) {
        if (filePath.find("nmap") != String::npos) {
            // Normal map - create blue-ish texture
            result = CreateSolidColor(256, 256, 128, 128, 255, 255);
            Platform::OutputDebugMessage("TextureLoader: Created normal map fallback for: " + filePath + "\n");
        } else {
            // Diffuse texture - create brick-like pattern
            result = CreateTestPattern(256, 256, "checkerboard");
            Platform::OutputDebugMessage("TextureLoader: Created brick pattern fallback for: " + filePath + "\n");
        }
    } else {
        // Generic fallback
        result = CreateTestPattern(128, 128, "checkerboard");
        Platform::OutputDebugMessage("TextureLoader: Created generic fallback for: " + filePath + "\n");
    }
    
    return result;
}

bool TextureLoader::ValidateBMPHeaders(const BMPFileHeader& fileHeader, const BMPInfoHeader& infoHeader) {
    // Check BMP signature
    if (fileHeader.bfType != 0x4D42) { // 'BM'
        return false;
    }
    
    // Check if supported bit depth
    if (infoHeader.biBitCount != 24 && infoHeader.biBitCount != 32) {
        Platform::OutputDebugMessage("TextureLoader: Unsupported bit depth: " + std::to_string(infoHeader.biBitCount) + "\n");
        return false;
    }
    
    // Check if uncompressed
    if (infoHeader.biCompression != 0) {
        Platform::OutputDebugMessage("TextureLoader: Compressed BMP not supported\n");
        return false;
    }
    
    return true;
}

void TextureLoader::ConvertToRGBA(const uint8* srcData, uint8* dstData, uint32 width, uint32 height, uint16 bitCount) {
    uint32 bytesPerPixel = bitCount / 8;
    uint32 rowSize = ((bitCount * width + 31) / 32) * 4; // BMP row padding
    
    for (uint32 y = 0; y < height; ++y) {
        const uint8* srcRow = srcData + y * rowSize;
        uint8* dstRow = dstData + y * width * 4;
        
        for (uint32 x = 0; x < width; ++x) {
            const uint8* srcPixel = srcRow + x * bytesPerPixel;
            uint8* dstPixel = dstRow + x * 4;
            
            if (bitCount == 24) {
                // BGR to RGBA
                dstPixel[0] = srcPixel[2]; // R
                dstPixel[1] = srcPixel[1]; // G
                dstPixel[2] = srcPixel[0]; // B
                dstPixel[3] = 255;         // A
            } else if (bitCount == 32) {
                // BGRA to RGBA
                dstPixel[0] = srcPixel[2]; // R
                dstPixel[1] = srcPixel[1]; // G
                dstPixel[2] = srcPixel[0]; // B
                dstPixel[3] = srcPixel[3]; // A
            }
        }
    }
}

void TextureLoader::FlipImageVertically(uint8* data, uint32 width, uint32 height, uint32 channels) {
    uint32 rowSize = width * channels;
    auto tempRow = std::make_unique<uint8[]>(rowSize);
    
    for (uint32 y = 0; y < height / 2; ++y) {
        uint8* topRow = data + y * rowSize;
        uint8* bottomRow = data + (height - 1 - y) * rowSize;
        
        // Swap rows
        memcpy(tempRow.get(), topRow, rowSize);
        memcpy(topRow, bottomRow, rowSize);
        memcpy(bottomRow, tempRow.get(), rowSize);
    }
}

// Factory methods for creating test textures
TextureImageData TextureLoader::CreateTestPattern(uint32 width, uint32 height, const String& pattern) {
    if (pattern == "checkerboard") {
        TextureImageData result;
        result.width = width;
        result.height = height;
        result.channels = 4;
        result.pixels = std::make_unique<uint8[]>(result.GetDataSize());
        
        uint32 checkSize = std::max(1u, std::min(width, height) / 8);
        
        for (uint32 y = 0; y < height; ++y) {
            for (uint32 x = 0; x < width; ++x) {
                bool isWhite = ((x / checkSize) + (y / checkSize)) % 2 == 0;
                uint8* pixel = result.pixels.get() + (y * width + x) * 4;
                
                pixel[0] = isWhite ? 255 : 0;   // R
                pixel[1] = isWhite ? 255 : 0;   // G
                pixel[2] = isWhite ? 255 : 0;   // B
                pixel[3] = 255;                 // A
            }
        }
        
        return result;
    }
    
    // Default to solid color if pattern not recognized
    return CreateSolidColor(width, height, 255, 0, 255); // Magenta
}

TextureImageData TextureLoader::CreateSolidColor(uint32 width, uint32 height, uint8 r, uint8 g, uint8 b, uint8 a) {
    TextureImageData result;
    result.width = width;
    result.height = height;
    result.channels = 4;
    result.pixels = std::make_unique<uint8[]>(result.GetDataSize());
    
    for (uint32 i = 0; i < width * height; ++i) {
        uint8* pixel = result.pixels.get() + i * 4;
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
        pixel[3] = a;
    }
    
    return result;
}

TextureImageData TextureLoader::CreateGradient(uint32 width, uint32 height) {
    TextureImageData result;
    result.width = width;
    result.height = height;
    result.channels = 4;
    result.pixels = std::make_unique<uint8[]>(result.GetDataSize());
    
    for (uint32 y = 0; y < height; ++y) {
        for (uint32 x = 0; x < width; ++x) {
            uint8* pixel = result.pixels.get() + (y * width + x) * 4;
            
            pixel[0] = static_cast<uint8>((x * 255) / width);   // R gradient
            pixel[1] = static_cast<uint8>((y * 255) / height);  // G gradient
            pixel[2] = 128;                                     // B constant
            pixel[3] = 255;                                     // A full
        }
    }
    
    return result;
}

TextureImageData TextureLoader::CreateUVTest(uint32 width, uint32 height) {
    TextureImageData result;
    result.width = width;
    result.height = height;
    result.channels = 4;
    result.pixels = std::make_unique<uint8[]>(result.GetDataSize());
    
    for (uint32 y = 0; y < height; ++y) {
        for (uint32 x = 0; x < width; ++x) {
            uint8* pixel = result.pixels.get() + (y * width + x) * 4;
            
            // UV coordinates as colors
            float u = static_cast<float>(x) / static_cast<float>(width - 1);
            float v = static_cast<float>(y) / static_cast<float>(height - 1);
            
            pixel[0] = static_cast<uint8>(u * 255);  // U as red
            pixel[1] = static_cast<uint8>(v * 255);  // V as green
            pixel[2] = 0;                            // Blue = 0
            pixel[3] = 255;                          // Alpha = 1
        }
    }
    
    return result;
}