#ifndef TESTING_UTILITIES_HPP
#define TESTING_UTILITIES_HPP
#include <bit>
#include <vector>
namespace
{
template<typename T>
std::string pack(const T *data, const int nSamples, const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::string result;
    if (nSamples < 1){return result;}
    result.resize(dataTypeSize*nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        unsigned char cArray[dataTypeSize];
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                      result.data() + dataTypeSize*i);
        }
    }
    else
    {
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::reverse_copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                              result.data() + dataTypeSize*i);
        }
    }
    return result;
}

template<typename T>
std::string pack(const std::vector<T> &data, const bool swapBytes)
{
    return pack(data.data(), data.size(), swapBytes);
}

template<typename T>
std::string pack(const std::vector<T> &data)
{
    const bool swapBytes
    {
        std::endian::native == std::endian::little ? false : true
    };
    return pack(data, swapBytes);
}

}
#endif
