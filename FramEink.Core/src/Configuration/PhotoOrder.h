#pragma once

#include <cstdint>
#include <cstring>

namespace frameink {

enum class PhotoOrder : uint8_t { Random = 0, Alphabetical = 1 };
constexpr PhotoOrder defaultPhotoOrder = PhotoOrder::Random;

inline bool validPhotoOrder(PhotoOrder order)
{
    return order == PhotoOrder::Random || order == PhotoOrder::Alphabetical;
}

inline const char *photoOrderName(PhotoOrder order)
{
    return order == PhotoOrder::Alphabetical ? "alphabetical" : "random";
}

inline bool parsePhotoOrder(const char *text, PhotoOrder &order)
{
    if (text == nullptr)
        return false;
    if (std::strcmp(text, "random") == 0)
        order = PhotoOrder::Random;
    else if (std::strcmp(text, "alphabetical") == 0)
        order = PhotoOrder::Alphabetical;
    else
        return false;
    return true;
}

// Optional extension after the existing update-minute block. Missing/invalid
// data defaults to Random without changing credentials, pause or update minute.
constexpr int photoOrderStorageBytes = 5;

template<class Storage>
PhotoOrder readPhotoOrder(Storage &storage, int offset, int capacity)
{
    if (offset < 0 || offset > capacity - photoOrderStorageBytes ||
        storage.read(offset) != 'F' || storage.read(offset + 1) != 'P' || storage.read(offset + 2) != 1)
        return defaultPhotoOrder;
    const uint8_t value = storage.read(offset + 3);
    const PhotoOrder order = static_cast<PhotoOrder>(value);
    const uint8_t check = uint8_t('F' ^ 'P' ^ 1 ^ value);
    return validPhotoOrder(order) && check == storage.read(offset + 4) ? order : defaultPhotoOrder;
}

template<class Storage>
bool writePhotoOrder(Storage &storage, int offset, int capacity, PhotoOrder order)
{
    if (!validPhotoOrder(order) || offset < 0 || offset > capacity - photoOrderStorageBytes)
        return false;
    const uint8_t value = static_cast<uint8_t>(order);
    const uint8_t bytes[] = {'F', 'P', 1, value, uint8_t('F' ^ 'P' ^ 1 ^ value)};
    for (uint8_t byte : bytes)
        storage.write(offset++, byte);
    return true;
}

} // namespace frameink
