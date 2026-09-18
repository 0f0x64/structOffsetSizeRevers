#pragma warning(disable:4996) 
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <type_traits>
#include <cstdint>
#include <cmath>

struct HeavyObject {
    std::vector<int> internalHeapData{ 1, 2, 3, 4, 5 };
    void* rawPointer = reinterpret_cast<void*>(0xDEADBEEF);
};

struct UserPrimitive {
    char r = 10;
    int g = 20;
    char r2 = 30;
    int64_t i64 = 1234567890LL;
    HeavyObject drawerArray;
    float f = 3.14f;
    double d = 9.81;
};

struct ScalarFieldOffset {
    std::string token;
    size_t offset = 0;
    size_t size = 0;
};

template <typename T>
bool TryParseText(const std::string& text, T& outVal) {
    std::stringstream ss(text);
    if constexpr (std::is_same_v<T, char> || std::is_same_v<T, unsigned char>) {
        int temp; if (ss >> temp) { outVal = static_cast<T>(temp); return true; }
    }
    else {
        if (ss >> outVal) return true;
    }
    return false;
}

bool BuildMemoryLayout(const uint8_t* originalMemory, size_t structSize,
    const std::vector<std::string>& tokens,
    std::vector<ScalarFieldOffset>& outLayout)
{
    std::cout << "[SCANNER] Size of struct: " << structSize << " bytes.\n";
    size_t currentByteOffset = 0;

    for (size_t tIdx = 0; tIdx < tokens.size(); ++tIdx) {
        const std::string& token = tokens[tIdx];
        std::cout << "[SCANNER] token[" << tIdx << "]: '" << token << "'\n";

        if (token == "?") {
            std::cout << "[SCANNER] -> Skip object field.\n";
            currentByteOffset += sizeof(HeavyObject);
            continue;
        }

        bool fieldFound = false;

        for (; currentByteOffset < structSize; ++currentByteOffset) {
            const size_t sizesToCheck[] = { 8, 4, 2, 1 };

            for (size_t size : sizesToCheck) {
                if (currentByteOffset + size > structSize) continue;

                bool textMatch = false;
                if (size == 1) {
                    char val; if (TryParseText(token, val)) textMatch = (*(originalMemory + currentByteOffset) == static_cast<uint8_t>(val));
                }
                else if (size == 2) {
                    short val; if (TryParseText(token, val)) textMatch = (*reinterpret_cast<const short*>(originalMemory + currentByteOffset) == val);
                }
                else if (size == 4) {
                    int valI; float valF;
                    if (TryParseText(token, valF) && std::abs(*reinterpret_cast<const float*>(originalMemory + currentByteOffset) - valF) < 0.0001f) textMatch = true;
                    else if (TryParseText(token, valI) && *reinterpret_cast<const int*>(originalMemory + currentByteOffset) == valI) textMatch = true;
                }
                else if (size == 8) {
                    long long valI; double valD;
                    if (TryParseText(token, valD) && std::abs(*reinterpret_cast<const double*>(originalMemory + currentByteOffset) - valD) < 0.0001) textMatch = true;
                    else if (TryParseText(token, valI) && *reinterpret_cast<const long long*>(originalMemory + currentByteOffset) == valI) textMatch = true;
                }

                if (!textMatch) continue;

                std::vector<uint8_t> sandbox(originalMemory, originalMemory + structSize);
                uint8_t* testPtr = sandbox.data() + currentByteOffset;
                bool verifySuccess = false;

                if (size == 8) {
                    *reinterpret_cast<uint64_t*>(testPtr) ^= 0x8000000000000000ULL;
                    verifySuccess = true;
                }
                else if (size == 4) {
                    *reinterpret_cast<uint32_t*>(testPtr) ^= 0x80000000;
                    verifySuccess = true;
                }
                else if (size == 2) {
                    *reinterpret_cast<uint16_t*>(testPtr) ^= 0x8000;
                    verifySuccess = true;
                }
                else if (size == 1) {
                    *testPtr ^= 0x80;
                    verifySuccess = true;
                }

                if (verifySuccess) {
                    ScalarFieldOffset field;
                    field.token = token;
                    field.offset = currentByteOffset;
                    field.size = size;
                    outLayout.push_back(field);
                    std::cout << "[SUCCESS] Offset: " << field.offset << ", Size: " << field.size << " bytes\n";
                    currentByteOffset += size;
                    fieldFound = true;
                    break;
                }
            }
            if (fieldFound) break;
        }
        if (!fieldFound) return false;
    }
    return true;
}

int main() {
    UserPrimitive colorStruct;
    std::vector<std::string> vsTokens = { "10", "20", "30", "1234567890", "?", "3.14", "9.81" };
    std::vector<ScalarFieldOffset> detectedLayout;
    const uint8_t* structBasePtr = reinterpret_cast<const uint8_t*>(&colorStruct);

    bool success = BuildMemoryLayout(structBasePtr, sizeof(UserPrimitive), vsTokens, detectedLayout);

    if (success) {
        std::cout << "\n--- Final Layout Results (Default Pack) ---\n";
        for (const auto& field : detectedLayout) {
            std::cout << "Text: " << field.token << " | Offset: " << field.offset << " | Size: " << field.size << "\n";
        }
    }
    return 0;
}
