/*
 * @File: PatternScanner.cpp
 * @Author: Aldrin John O. Manalansan (ajom)
 * @Email: aldrinjohnolaermanalansan@gmail.com
 * @Brief: Implementation of single-file Array of Byte Pattern Scanner
 * @LastUpdate: October 20, 2025
 *
 * Copyright (C) 2025  Aldrin John O. Manalansan  <aldrinjohnolaermanalansan@gmail.com>
 *
 * This Source Code is served under Open-Source AJOM License
 * You should have received a copy of License_OS-AJOM
 * along with this source code. If not, see:
 * <https://raw.githubusercontent.com/Aldrin-John-Olaer-Manalansan/AJOM_License/refs/heads/main/LICENSE_AJOM-OS>
 */

#include "./PatternScanner.hpp"

#include <cstdint>
#include <ctype.h>
#include <iostream>

namespace PatternScanner {

    // Helper function to convert a single hex character to its integer value
    static int8_t CharToHex(const char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        } else if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        std::cout << "Invalid hex character";
        return -1;
    }

    /**
     * @brief Convert a string Pattern to an Array of Byte Pattern.
     * @param[in] pattern The string pattern to be converted.
     * @return A vector of shorts where each element represents a byte of the
     *         pattern. If a byte is represented by a '?', it is replaced with
     *         -1 in the vector.
     */
    std::vector<short> t_Pattern::PatternToAoB(const char* pattern) {
        // convert string Pattern to Array of Byte Pattern
        std::vector<short> AoB;
        while (*pattern) {
            if (isspace((unsigned char)*pattern)) {
                ++pattern; // skip separator
                continue;
            }
            if (*pattern == '?') { // any byte
                ++pattern;
                AoB.push_back(-1); // indicate to accept any byte
                continue;
            }
            char hex = CharToHex(*pattern++);
            if ((hex == -1) // Invalid most-significant hex character
            || !*pattern // null terminator reached
            || isspace((unsigned char)*pattern)) {
                // incomplete hex
                AoB.push_back(-1);
                continue;
            }
            short byte = (hex << 4) & 0xF0;
            hex = CharToHex(*pattern++);
            if (hex == -1) { // Invalid most-significant hex character
                // incomplete hex
                AoB.push_back(-1);
                continue;
            }
            byte |= hex & 0xF;
            AoB.push_back(byte);
        }
        return AoB;
    }

/**
 * @brief Process a single byte of the input stream.
 * @param[in] api Pointer to the API struct.
 * @param[in] processedByteIndex The index of the byte being processed.
 * @param[in,out] processedByte The byte being processed.
 * @return True if the initial occurence of the pattern has been found and stop iterating.
 *         False otherwise.
 * 
 * This function is responsible for processing a single byte of the input stream.
 * It compares the processed byte with the current byte of the pattern. If
 * there is a mismatch, it resets the seek index to 0. If there is a
 * match, it increments the seek index and checks if we have reached the end
 * of the pattern. If we have, it increments the occurence count and resets the
 * seek index to 0 to search for the next occurence. If the countOccurence is
 * false, it means we only need to find one occurence and stop iterating.
 */
    bool t_Pattern::ProcessByte(t_Pattern::t_API *const api, const size_t processedByteIndex, uint8_t& processedByte) {
        const auto comparedByte = api->AoB[api->seekIndex];
        if ((comparedByte != -1) // not any byte
        && (processedByte != (comparedByte & 0xFF))) { // mismatch
            if (api->seekIndex) {
                api->seekIndex = 0; // reset
            }
            return false; // keep iterating
        }
        api->seekIndex++; // move to next byte
        const auto maxPatternSize = api->AoB.size();
        if (api->seekIndex >= maxPatternSize) { // we already reached the end of the pattern
            if (*api->foundOffset == SIZE_MAX) { // not found yet
                *api->foundOffset = processedByteIndex - maxPatternSize + 1;
            }
            api->occurenceCount++;
            if (!api->countOccurence) {
                return true; // initial occurence found, stop iterating
            }
            api->seekIndex = 0; // reset to search for next occurence
        }
        return false; // keep iterating
    }

    bool t_Pattern::CheckInitialGuessedOffset(const std::span<uint8_t>& data) {
        if (m_FoundOffset == SIZE_MAX) { // has no initial guessed offset
            return false;
        }
        t_API api = {m_AoB, &m_FoundOffset, false, 0, 0};
        for (size_t inspectedByteIndex = m_FoundOffset; inspectedByteIndex < data.size(); inspectedByteIndex++) {
            if (ProcessByte(&api, inspectedByteIndex, data[inspectedByteIndex])) {
                return 1; // initial guess matches the pattern
            }
        }
        return false;
    }
    
    bool t_Pattern::CheckInitialGuessedOffset(t_Iterator iterator) {
        // check if initial guessed offset is correct
        if (m_FoundOffset == SIZE_MAX) { // has no initial guessed offset
            return false;
        }
        size_t processedByteIndex = m_FoundOffset; // initial guess
        if (!iterator(&processedByteIndex, nullptr)) { // failed to set iterator's index to processedByteIndex
            return false;
        }
        t_API api = {m_AoB, &m_FoundOffset, false, 0, 0};
        uint8_t processedByte;
        while (iterator(&processedByteIndex, &processedByte)) {
            if (ProcessByte(&api, processedByteIndex, processedByte)) {
                return true; // initial guess matches the pattern
            }
        }
        return false;
    }
    
    bool t_Pattern::CheckInitialGuessedOffset(void* const processorData, const t_Processor processor) {
        if (m_FoundOffset == SIZE_MAX) { // has no initial guessed offset
            return false;
        }
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        t_API api = {m_AoB, &m_FoundOffset, false, 0, 0};
        processorAPI->searchedOffset = m_FoundOffset;

        auto oldSearchSize = processorAPI->searchSize;
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        processorAPI->searchSize = oldSearchSize;

        return api.occurenceCount ? true : false;
    }

    /**
     * @brief Scan a given byte array for all patterns in the pattern list
     * @param data The byte array to be scanned
     * @param countOccurence If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A vector of size_t containing the number of times each pattern occurs in the data. The index of the vector corresponds to the index of the pattern in the pattern list. If countOccurence is false, all elements of the vector except the first one will be zero.
     */
    size_t t_Pattern::Scan(const std::span<uint8_t>& data, const bool countOccurence) {
        if (!countOccurence && CheckInitialGuessedOffset(data)) {
            return 1;
        }
        // search byte by byte
        t_API api = {m_AoB, &m_FoundOffset, countOccurence, 0, 0};
        m_FoundOffset = SIZE_MAX; // initially not found
        for (size_t inspectedByteIndex = 0; inspectedByteIndex < data.size(); inspectedByteIndex++) {
            if (ProcessByte(&api, inspectedByteIndex, data[inspectedByteIndex])) {
                break;
            }
        }
        return api.occurenceCount;
    }

    /**
     * @brief Scan a given byte array for all occurrences of the pattern.
     * @param data The byte array to be scanned.
     * @param size The size of the byte array.
     * @param countOccurence If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A size_t containing the number of times the pattern occurs in the data. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    size_t t_Pattern::Scan(void* const data, const size_t size, const bool countOccurence) {
        return Scan(std::span<uint8_t>(static_cast<uint8_t*>(data), size), countOccurence);
    }

    /**
     * @brief Scan a given byte array for all occurrences of the pattern using a custom iterator.
     * @param iterator A custom iterator that:
     * - Stores the current iterated element's index of the inspected array, at outByteIndex.
     * - Stores the current iterated element's value of the inspected array, at outByte.
     * - The iterator must its index to the beginning of the array when outByteIndex and outByte = nullptr.
     * - Returns false if there are no more bytes to iterate(reached the end of the inspected array), true otherwise.
     * - The iterator should take two parameters: a pointer to size_t to store the index of the current byte, and a pointer to uint8_t to store the current byte.
     * @param countOccurence If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A size_t containing the number of times the pattern occurs in the data. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    size_t t_Pattern::Scan(t_Iterator iterator, const bool countOccurence) {
        if (!countOccurence && CheckInitialGuessedOffset(iterator)) {
            return 1;
        }
        size_t processedByteIndex = 0;
        if (!iterator(&processedByteIndex, nullptr)) { // failed to set iterator's index to processedByteIndex
            return 0;
        }
        t_API api = {m_AoB, &m_FoundOffset, countOccurence, 0, 0};
        m_FoundOffset = SIZE_MAX; // initially not found
        uint8_t processedByte;
        while (iterator(&processedByteIndex, &processedByte)) {
            if (ProcessByte(&api, processedByteIndex, processedByte)) {
                break;
            }
        }
        return api.occurenceCount;
    }

    /**
     * @brief Scan a given byte array for all occurrences of the pattern using a custom processor.
     * @param processorData A pointer to the byte array to be scanned.
     * @param processor A custom processor that takes three parameters: a void pointer to the byte array to be scanned, a t_API object that contains the pattern array, the found offset, and the occurrence count, and a t_Callback function that processes a single byte in the pattern matching process.
     * @param countOccurence If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A size_t containing the number of times the pattern occurs in the data. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    size_t t_Pattern::Scan(void* const processorData, const t_Processor processor, const bool countOccurence) {
        if (!countOccurence && CheckInitialGuessedOffset(processorData, processor)) {
            return 1;
        }
        t_API api = {m_AoB, &m_FoundOffset, countOccurence, 0, 0};
        m_FoundOffset = SIZE_MAX; // initially not found
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        processorAPI->searchedOffset = 0;
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        return api.occurenceCount;
    }

    /**
     * @brief Get the offset of the first occurrence of the pattern in a given data block.
     * @return The offset of the first occurrence of the pattern in a given data block.
     * @details The offset is relative to the start of the data block. If no occurrence is found, the offset is SIZE_MAX.
     */
    size_t t_Pattern::GetFoundOffset(void) {
        return m_FoundOffset;
    }

/**
 * @brief Process a single byte of the input stream.
 * @param api Pointer to the API struct.
 * @param processedByteIndex The index of the byte being processed.
 * @param processedByte The byte being processed.
 * @return True if the initial occurrence of all patterns has been found and stop iterating.
 *         False otherwise.
 * 
 * This function is responsible for processing a single byte of the input stream.
 * It compares the processed byte with the current byte of the pattern. If
 * there is a mismatch, it resets the seek index to 0. If there is a
 * match, it increments the seek index and checks if we have reached the end
 * of the pattern. If we have, it increments the occurrence count and resets the
 * seek index to 0 to search for the next occurrence. If the countOccurence is
 * false, it means we only need to find one occurrence and stop iterating.
 */
    bool t_PatternPtrList::ProcessByte(t_API *const api, const size_t processedByteIndex, uint8_t& processedByte) {
        for (size_t elementIndex = 0;
            elementIndex < api->list.size();
            elementIndex++
        ) {
            auto const patternPtr = api->list[elementIndex];
            if (!api->countOccurence // search only a single occurence
            && (patternPtr->m_FoundOffset != SIZE_MAX)) {  // already found
                continue;
            }
            auto& seekIndex = api->seekIndeces[elementIndex];
            const auto comparedByte = patternPtr->m_AoB[seekIndex];
            if ((comparedByte != -1) // not any byte
            && (processedByte != (comparedByte & 0xFF))) { // mismatch
                if (seekIndex) {
                    seekIndex = 0; // reset
                }
                continue;
            }
            seekIndex++; // move to next byte
            const auto maxPatternSize = patternPtr->m_AoB.size();
            if (seekIndex >= maxPatternSize) { // we already reached the end of the pattern
                if (patternPtr->m_FoundOffset == SIZE_MAX) { // not found yet
                    patternPtr->m_FoundOffset = processedByteIndex - maxPatternSize + 1;
                }
                api->occurenceCounts[elementIndex]++;
                seekIndex = 0; // reset to search for next occurence
            }
        }
        if (!api->countOccurence) { // search only a single occurence
            // check if all patterns have been found
            size_t foundCount = 0;
            for (auto& occurenceCount : api->occurenceCounts) {
                if (occurenceCount) { // at least one pattern has been found
                    foundCount++;
                }
            }
            if (foundCount == api->list.size()) { // all patterns have been found
                return true; // halt search
            }
        }
        return false; // keep iterating
    }

    /**
     * @brief Scan a given byte array for all patterns in the pattern list
     * @param data The byte array to be scanned
     * @param countOccurence If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A vector of size_t containing the number of times each pattern occurs in the data. The index of the vector corresponds to the index of the pattern in the pattern list. If countOccurence is false, all elements of the vector except the first one will be zero.
     */
    std::vector<size_t> t_PatternPtrList::Scan(const std::span<uint8_t> data, const bool countOccurence) {
        // initialize parameters
        t_API api = {m_list, countOccurence, std::vector<size_t>(m_list.size(), 0), std::vector<size_t>(m_list.size(), 0)};

        if (!countOccurence) {
            size_t foundCount = 0;
            for (size_t i = 0; i < m_list.size(); i++) {
                if (m_list[i]->CheckInitialGuessedOffset(data)) {
                    foundCount++;
                    api.occurenceCounts[i] = 1;
                }
            }
            if (foundCount == m_list.size()) {
                // all guessed offsets are correct, no need to perform deep scan anymore
                return api.occurenceCounts;
            }
        }

        // perform deep scan

        // initially not found
        for (size_t i = 0; i < m_list.size(); i++) {
            m_list[i]->m_FoundOffset = SIZE_MAX;
            api.occurenceCounts[i] = 0;
        }
        // search byte by byte
        for (size_t i = 0; i < data.size(); i++) {
            if (ProcessByte(&api, i, data[i])) {
                break;
            }
        }
        return api.occurenceCounts;
    }

    /**
     * @brief Search for all the patterns in the given data.
     * @param data The data to search in.
     * @param size The size of the data.
     * @param countOccurence If true, count the number of occurences of each pattern.
     * @return A vector containing the number of occurences of each pattern.
     */
    std::vector<size_t> t_PatternPtrList::Scan(void* const data, const size_t size, const bool countOccurence) {
        return Scan(std::span<uint8_t>(static_cast<uint8_t*>(data), size), countOccurence);
    }

/**
 * @brief Search for all the patterns in the given data using an iterator.
 * @details This function uses an iterator to traverse the given data and search for all the patterns in the pattern list.
 * @param iterator The iterator to use when traversing the given data.
 * @param countOccurence If true, count the number of occurences of each pattern.
 * @return A vector containing the number of occurences of each pattern.
 */
    std::vector<size_t> t_PatternPtrList::Scan(t_Iterator iterator, const bool countOccurence) {
        // initialize parameters
        t_API api = {m_list, countOccurence, std::vector<size_t>(m_list.size(), 0), std::vector<size_t>(m_list.size(), 0)};

        if (!countOccurence) {
            size_t foundCount = 0;
            for (size_t i = 0; i < m_list.size(); i++) {
                if (m_list[i]->CheckInitialGuessedOffset(iterator)) {
                    foundCount++;
                    api.occurenceCounts[i] = 1;
                }
            }
            if (foundCount == m_list.size()) {
                // all guessed offsets are correct, no need to perform deep scan anymore
                return api.occurenceCounts;
            }
        }

        // perform deep scan

        // initially not found
        for (size_t i = 0; i < m_list.size(); i++) {
            m_list[i]->m_FoundOffset = SIZE_MAX;
            api.occurenceCounts[i] = 0;
        }

        // search byte by byte
        size_t processedByteIndex;
        uint8_t processedByte;
        if (iterator(nullptr, nullptr)) { // reset iterator's index
            while (iterator(&processedByteIndex, &processedByte)) {
                if (ProcessByte(&api, processedByteIndex, processedByte)) {
                    break;
                }
            }
        }
        return api.occurenceCounts;
    }

/**
 * @brief Scan for all the patterns in the given data using a processor.
 * @details This function uses a processor to traverse the given data and search for all the patterns in the pattern list.
 * @param processorData The data to be processed by the processor.
 * @param processor The processor to use when traversing the given data.
 * @param countOccurence If true, count the number of occurences of each pattern.
 * @return A vector containing the number of occurences of each pattern.
 */
    std::vector<size_t> t_PatternPtrList::Scan(void* const processorData, const t_Processor processor, const bool countOccurence) {
        t_API api = {m_list, countOccurence, std::vector<size_t>(m_list.size(), 0), std::vector<size_t>(m_list.size(), 0)};

        if (!countOccurence) {
            size_t foundCount = 0;
            for (size_t i = 0; i < m_list.size(); i++) {
                if (m_list[i]->CheckInitialGuessedOffset(processorData, processor)) {
                    foundCount++;
                    api.occurenceCounts[i] = 1;
                }
            }
            if (foundCount == m_list.size()) {
                // all guessed offsets are correct, no need to perform deep scan anymore
                printf("All guessed pattern offsets are correct. No Changes in known memory layout.\n");
                return api.occurenceCounts;
            }
        }

        // perform deep scan
        printf("Changes detected to memory layout. Performing deep pattern scan\n");

        // initially not found
        for (size_t i = 0; i < m_list.size(); i++) {
            m_list[i]->m_FoundOffset = SIZE_MAX;
            api.occurenceCounts[i] = 0;
        }
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        return api.occurenceCounts;
    }

    /**
     * @brief Get the t_Pattern object at the given index.
     * @param index The index of the t_Pattern object to retrieve.
     * @return The t_Pattern object at the given index.
     */
    t_Pattern* t_PatternPtrList::operator[](const size_t index) const {
        return m_list[index];
    }
}