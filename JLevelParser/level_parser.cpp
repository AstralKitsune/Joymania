#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <iomanip>
#include <cstring>

struct ObjectRecord {
    std::string name;
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    uint8_t variant;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_level.dat> <output.csv>\n";
        return 1;
    }
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error opening input file: " << inputPath << "\n";
        return 1;
    }

    uint32_t recordCount = 0;
    in.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
    std::cout << "Found " << recordCount << " records.\n";

    std::ofstream out(outputPath);
    if (!out) {
        std::cerr << "Error creating output file: " << outputPath << "\n";
        return 1;
    }

    // CSV header
    out << "Name,MinX,MinY,MinZ,MaxX,MaxY,MaxZ,Variant\n";

    for (uint32_t i = 0; i < recordCount; ++i) {
        // Read name buffer (32 bytes)
        char nameBuf[32];
        in.read(nameBuf, sizeof(nameBuf));
        // Extract null-terminated C-string
        std::string name(nameBuf, strnlen(nameBuf, sizeof(nameBuf)));

        // Read 6 floats
        float coords[6];
        in.read(reinterpret_cast<char*>(coords), sizeof(coords));

        // Read variant byte
        uint8_t variant = 0;
        in.read(reinterpret_cast<char*>(&variant), sizeof(variant));

        // Skip padding (3 bytes)
        in.seekg(3, std::ios::cur);

        // Write to CSV
        out << '"' << name << "\","
            << std::fixed << std::setprecision(6)
            << coords[0] << ',' << coords[1] << ',' << coords[2] << ','
            << coords[3] << ',' << coords[4] << ',' << coords[5] << ','
            << static_cast<int>(variant)
            << "\n";
    }

    std::cout << "Parsing complete. Output written to " << outputPath << "\n";
    return 0;
}
