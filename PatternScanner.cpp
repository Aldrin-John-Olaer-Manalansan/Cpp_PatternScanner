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

    void t_Pattern::PrintInfo(void) {
        printf("Offset = %X\nAoB:\n", m_FoundOffset);
        uint8_t lineBreak = 0;
        for (const auto &byte : m_AoB) {
            if (byte == -1) {
                printf(" ? ");
            } else {
                printf("%.2X ", byte);
            }
            lineBreak++;
            if (lineBreak % 16 == 0) {
                printf("\n");
            }
        }
        printf("\n");
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
 * seek index to 0 to search for the next occurence. If the isDeepScan is
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
            api->seekIndex = 0; // reset to search for next occurence
            api->result.foundOffsets.push_back(processedByteIndex - maxPatternSize + 1);
            if (!api->result.isDeepScanned) {
                return true; // initial occurence found, stop iterating
            }
        }
        return false; // keep iterating
    }

    /**
     * @brief Checks if the initial guessed offset matches the pattern in the provided data span.
     * @param[in] data The span of bytes to scan for the pattern starting at the guessed offset.
     * @return True if the pattern matches at the guessed offset, false otherwise.
     *
     * This function verifies whether the pattern matches the data starting from the
     * guessed offset. It performs a scan over the specified range of bytes and
     * returns true if the pattern is found to match exactly at that location.
     * If the guessed offset is invalid (SIZE_MAX) or the range exceeds the data bounds,
     * the function returns false without performing a scan.
     */
    bool t_Pattern::CheckInitialGuessedOffset(const std::span<uint8_t>& data) {
        if (m_FoundOffset == SIZE_MAX) { // has no initial guessed offset
            return false;
        }
        size_t end = m_FoundOffset + m_AoB.size();
        if (end > data.size()) {
            return false; // out of bounds
        }
        ScanResult result = {std::vector<size_t>(0, 0), false};
        t_API api = {m_AoB, 0, 0, result};
        for (size_t inspectedByteIndex = m_FoundOffset; inspectedByteIndex < end; inspectedByteIndex++) {
            if (ProcessByte(&api, inspectedByteIndex, data[inspectedByteIndex])) {
                return true; // initial guess matches the pattern
            }
        }
        return false;
    }
    
    /**
     * @brief Checks if the initial guessed offset matches the pattern using an iterator function.
     * @param[in] iterator A function pointer to an iterator that provides byte data and index updates.
     * @return True if the pattern matches at the guessed offset, false otherwise.
     *
     * This function verifies whether the pattern matches the data starting from the
     * guessed offset using the provided iterator function. It iterates through the
     * data using the iterator and checks for a match against the pattern. If the
     * guessed offset is invalid (SIZE_MAX) or the iterator fails to set the index,
     * the function returns false without performing a scan.
     */
    bool t_Pattern::CheckInitialGuessedOffset(t_Iterator iterator) {
        // check if initial guessed offset is correct
        if (m_FoundOffset == SIZE_MAX) {
            return false; // has no initial guessed offset
        }
        size_t processedByteIndex = m_FoundOffset; // initial guess
        if (!iterator(&processedByteIndex, nullptr)) {
            return false; // failed to set iterator's index to processedByteIndex
        }
        ScanResult result = {std::vector<size_t>(0, 0), false};
        t_API api = {m_AoB, 0, 0, result};
        uint8_t processedByte;
        while (iterator(&processedByteIndex, &processedByte)) {
            if (ProcessByte(&api, processedByteIndex, processedByte)) {
                return true; // initial guess matches the pattern
            }
        }
        return false;
    }
    
    /**
     * @brief Checks if the initial guessed offset matches the pattern using a processor function.
     * @param[in] processorData Pointer to the processor data structure.
     * @param[in] processor Function pointer to the processor that scans the data and calls ProcessByte.
     * @return True if the pattern matches at the guessed offset, false otherwise.
     *
     * This function verifies whether the pattern matches the data starting from the
     * guessed offset using the provided processor function. It temporarily sets up
     * the processor's search parameters to match the guessed offset and pattern size,
     * then calls the processor to scan and validate the match. The original search
     * parameters are restored after the check. If the guessed offset is invalid (SIZE_MAX),
     * the function returns false without performing a scan.
     */
    bool t_Pattern::CheckInitialGuessedOffset(void* const processorData, const t_Processor processor) {
        if (m_FoundOffset == SIZE_MAX) { // has no initial guessed offset
            return false;
        }
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        ScanResult result = {std::vector<size_t>(0, 0), false};
        t_API api = {m_AoB, 0, 0, result};

        auto oldsearchedOffset = processorAPI->searchedOffset;
        auto oldSearchSize = processorAPI->searchSize;
        processorAPI->searchedOffset = m_FoundOffset;
        processorAPI->searchSize = m_AoB.size();
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        processorAPI->searchedOffset = oldsearchedOffset;
        processorAPI->searchSize = oldSearchSize;

        return result.foundOffsets.size();
    }

    /**
     * @brief Scan a given byte array for all patterns in the pattern list
     * @param data The byte array to be scanned
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A vector of size_t containing the number of times each pattern occurs in the data. The index of the vector corresponds to the index of the pattern in the pattern list. If isDeepScan is false, all elements of the vector except the first one will be zero.
     */
    t_Pattern::ScanResult t_Pattern::Scan(const std::span<uint8_t>& data, const bool isDeepScan) {
        ScanResult result = {std::vector<size_t>(0, 0), isDeepScan};
        if (!isDeepScan) {
            if (CheckInitialGuessedOffset(data)) {
                result.foundOffsets.resize(1);
                result.foundOffsets[0] = m_FoundOffset;
                return result;
            }
            result.isDeepScanned = true; // will perform deep scan
        }
        // search byte by byte
        t_API api = {m_AoB, 0, 0, result};
        for (size_t inspectedByteIndex = 0; inspectedByteIndex < data.size(); inspectedByteIndex++) {
            if (ProcessByte(&api, inspectedByteIndex, data[inspectedByteIndex])) {
                break;
            }
        }
        m_FoundOffset = result.foundOffsets.size() ? result.foundOffsets[0] : SIZE_MAX;
        return result;
    }

    /**
     * @brief Scan a given byte array for all occurrences of the pattern.
     * @param data The byte array to be scanned.
     * @param size The size of the byte array.
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A size_t containing the number of times the pattern occurs in the data. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    t_Pattern::ScanResult t_Pattern::Scan(void* const data, const size_t size, const bool isDeepScan) {
        return Scan(std::span<uint8_t>(static_cast<uint8_t*>(data), size), isDeepScan);
    }

    /**
     * @brief Scan a given byte array for all occurrences of the pattern using a custom iterator.
     * @param iterator A custom iterator that:
     * - Stores the current iterated element's index of the inspected array, at outByteIndex.
     * - Stores the current iterated element's value of the inspected array, at outByte.
     * - The iterator must its index to the beginning of the array when outByteIndex and outByte = nullptr.
     * - Returns false if there are no more bytes to iterate(reached the end of the inspected array), true otherwise.
     * - The iterator should take two parameters: a pointer to size_t to store the index of the current byte, and a pointer to uint8_t to store the current byte.
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A size_t containing the number of times the pattern occurs in the data. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    t_Pattern::ScanResult t_Pattern::Scan(t_Iterator iterator, const bool isDeepScan) {
        ScanResult result = {std::vector<size_t>(0, 0), isDeepScan};
        if (!isDeepScan) {
            if (CheckInitialGuessedOffset(iterator)) {
                result.foundOffsets.resize(1);
                result.foundOffsets[0] = m_FoundOffset;
                return result;
            }
            result.isDeepScanned = true;
        }
        size_t processedByteIndex = 0;
        if (!iterator(&processedByteIndex, nullptr)) { // failed to set iterator's index to processedByteIndex
            return result;
        }
        t_API api = {m_AoB, 0, 0, result};
        uint8_t processedByte;
        while (iterator(&processedByteIndex, &processedByte)) {
            if (ProcessByte(&api, processedByteIndex, processedByte)) {
                break;
            }
        }
        m_FoundOffset = result.foundOffsets.size() ? result.foundOffsets[0] : SIZE_MAX;
        return result;
    }
    
    /**
     * @brief Scan a given byte array for all occurrences of the pattern using a custom processor.
     * @param processorData A pointer to data used by the processor function.
     * @param processor A function that iterates through the data and calls ProcessByte for each byte.
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A ScanResult struct containing the number of times the pattern occurs in the data and the offsets of those occurrences. The offset of the first occurrence of the pattern can be retrieved with GetFoundOffset().
     */
    t_Pattern::ScanResult t_Pattern::Scan(void* const processorData, const t_Processor processor, const bool isDeepScan) {
        ScanResult result = {std::vector<size_t>(0, 0), isDeepScan};
        if (!isDeepScan) {
            if (CheckInitialGuessedOffset(processorData, processor)) {
                result.foundOffsets.resize(1);
                result.foundOffsets[0] = m_FoundOffset;
                return result;
            }
            result.isDeepScanned = true;
        }
        t_API api = {m_AoB, 0, 0, result};
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        processorAPI->searchedOffset = 0;
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        m_FoundOffset = result.foundOffsets.size() ? result.foundOffsets[0] : SIZE_MAX;
        return result;
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
     * seek index to 0 to search for the next occurrence. If the isDeepScan is
     * false, it means we only need to find one occurrence and stop iterating.
     */
    bool t_PatternPtrList::ProcessByte(t_API *const api, const size_t processedByteIndex, uint8_t& processedByte) {
        for (size_t elementIndex = 0;
            elementIndex < api->list.size();
            elementIndex++
        ) {
            auto const patternPtr = api->list[elementIndex];
            if (!api->result.isDeepScanned // search only a single occurence
            && api->result.foundOffsets.size()) {  // already found
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
                seekIndex = 0; // reset to search for next occurence
                api->result.foundOffsets[elementIndex].push_back(processedByteIndex - maxPatternSize + 1); // append to list of found offsets
                printf("Pattern %u Found at %X. Count: %u\n", elementIndex, processedByteIndex - maxPatternSize + 1, api->result.foundOffsets[elementIndex].size());
                // patternPtr->PrintInfo();
            }
        }
        if (!api->result.isDeepScanned) { // search only a single occurence
            // check if all patterns have been found
            for (auto& foundOffset : api->result.foundOffsets) {
                if (!foundOffset.size()) { // no pattern has been found
                    return false; // keep iterating
                }
            }
            return true; // all patterns have been found
        }
        return false; // keep iterating
    }

    /**
     * @brief Checks the initial guessed offsets for all patterns in the list using a custom processor.
     * @param processorData A pointer to the byte array to be scanned.
     * @param processor A custom processor that takes three parameters: a void pointer to the byte array to be scanned, a t_API object that contains the pattern array, the found offset, and the occurrence count, and a t_Callback function that processes a single byte in the pattern matching process.
     * @return A vector of booleans indicating whether the initial guessed offset was valid for each pattern in the list. A value of true means the guessed offset was valid, false otherwise.
     */
    std::vector<bool> t_PatternPtrList::CheckInitialGuessedOffsets(void* const processorData, const t_Processor processor) {
        const size_t count = m_list.size();
        std::vector<bool> result(count, false); 
        for (size_t i = 0; i < count; i++) {
            result[i] = m_list[i]->CheckInitialGuessedOffset(processorData, processor);
        }
        return result;
    }

    /**
     * @brief Scan a given byte array for all patterns in the pattern list
     * @param data The byte array to be scanned
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A vector of size_t containing the number of times each pattern occurs in the data. The index of the vector corresponds to the index of the pattern in the pattern list. If isDeepScan is false, all elements of the vector except the first one will be zero.
     */
    t_PatternPtrList::ScanResult t_PatternPtrList::Scan(const std::span<uint8_t> data, const bool isDeepScan) {
        // Initialize the result structure with appropriate size and scan type
        ScanResult result = {std::vector<std::vector<size_t>>(m_list.size()), isDeepScan};

        // If not performing a deep scan, check initial guessed offsets first
        if (!isDeepScan) {
            for (size_t i = 0; i < m_list.size(); i++) {
                // If any pattern's guessed offset is incorrect, a deep scan is required
                if (!m_list[i]->CheckInitialGuessedOffset(data)) {
                    result.isDeepScanned = true;
                    break;
                }
            }
            // If all guessed offsets are correct, no need to perform deep scan
            if (!result.isDeepScanned) {
                for (size_t i = 0; i < m_list.size(); i++) {
                    result.foundOffsets[i].resize(1);
                    result.foundOffsets[i][0] = m_list[i]->GetFoundOffset();
                }
                printf("Processor: All guessed pattern offsets are correct. No Changes in known memory layout.\n");
                return result;
            }

            // Deep scan is required due to changes in memory layout
            printf("Processor: Changes detected to memory layout.\n");
        }

        // Perform deep scan
        printf("Processor: Performing deep pattern scan\n");

        // Initialize API structure for deep scan
        t_API api = {m_list, std::vector<size_t>(m_list.size(), 0), result};

        // search byte by byte
        for (size_t i = 0; i < data.size(); i++) {
            if (ProcessByte(&api, i, data[i])) {
                break;
            }
        }
        for (size_t i = 0; i < m_list.size(); i++) {
            // set foundoffset to the first found offset if any or SIZE_MAX if not found
            m_list[i]->m_FoundOffset = result.foundOffsets[i].size() ? result.foundOffsets[i][0] : SIZE_MAX;
        }

        return result;
    }

    /**
     * @brief Scan a given byte array for all patterns in the pattern list
     * @param data The byte array to be scanned
     * @param size The size of the byte array to be scanned
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A ScanResult struct containing the number of times each pattern occurs in the data and the offsets of each pattern's occurrences. The index of the occurence counts and offsets corresponds to the index of the pattern in the pattern list. If isDeepScan is false, only the first pattern's occurrence count and offsets will be populated.
     */
    t_PatternPtrList::ScanResult t_PatternPtrList::Scan(void* const data, const size_t size, const bool isDeepScan) {
        return Scan(std::span<uint8_t>(static_cast<uint8_t*>(data), size), isDeepScan);
    }

    /**
     * @brief Scan a given data stream using an iterator for all patterns in the pattern list
     * @param iterator The iterator to use for traversing the data stream
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurrence of each pattern.
     * @return A ScanResult struct containing the found offsets for each pattern and a flag indicating if a deep scan was performed
     */
    t_PatternPtrList::ScanResult t_PatternPtrList::Scan(t_Iterator iterator, const bool isDeepScan) {
        ScanResult result = {std::vector<std::vector<size_t>>(m_list.size()), isDeepScan};
        
        if (!isDeepScan) {
            for (size_t i = 0; i < m_list.size(); i++) {
                if (!m_list[i]->CheckInitialGuessedOffset(iterator)) {
                    // atleast one among the initial guessed pattern was not found. Require Deepscan
                    result.isDeepScanned = true;
                    break;
                }
            }
            if (!result.isDeepScanned) {
                // all guessed offsets are correct, no need to perform deep scan anymore
                for (size_t i = 0; i < m_list.size(); i++) {
                    result.foundOffsets[i].resize(1);
                    result.foundOffsets[i][0] = m_list[i]->GetFoundOffset();
                }
                printf("Iterator: All guessed pattern offsets are correct. No Changes in known memory layout.\n");
                return result;
            }

            // perform deep scan
            printf("Iterator: Changes detected to memory layout.\n");
        }

        // perform deep scan

        printf("Iterator: Performing deep pattern scan using iterator\n");

        t_API api = {m_list, std::vector<size_t>(m_list.size(), 0), result};
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
        for (size_t i = 0; i < m_list.size(); i++) {
            // set foundoffset to the first found offset if any or SIZE_MAX if not found
            m_list[i]->m_FoundOffset = result.foundOffsets[i].size() ? result.foundOffsets[i][0] : SIZE_MAX;
        }
        return result;
    }

    /**
     * @brief Scan a given data using a processor function for all patterns in the pattern list
     * @param processorData The data to be processed by the processor function
     * @param processor The processor function to be used for scanning the data
     * @param isDeepScan If true, count the number of times each pattern occurs in the data. If false, only search for a single occurence of each pattern.
     * @return A ScanResult structure containing the found offsets and whether a deep scan was performed
     */
    t_PatternPtrList::ScanResult t_PatternPtrList::Scan(void* const processorData, const t_Processor processor, bool isDeepScan) {
        // Initialize the result structure with appropriate size and scan type
        ScanResult result = {std::vector<std::vector<size_t>>(m_list.size()), isDeepScan};

        // If not performing a deep scan, check initial guessed offsets first
        if (!isDeepScan) {
            for (size_t i = 0; i < m_list.size(); i++) {
                // If any pattern's guessed offset is incorrect, a deep scan is required
                if (!m_list[i]->CheckInitialGuessedOffset(processorData, processor)) {
                    result.isDeepScanned = true;
                    break;
                }
            }
            // If all guessed offsets are correct, no need to perform deep scan
            if (!result.isDeepScanned) {
                for (size_t i = 0; i < m_list.size(); i++) {
                    result.foundOffsets[i].resize(1);
                    result.foundOffsets[i][0] = m_list[i]->GetFoundOffset();
                }
                printf("Processor: All guessed pattern offsets are correct. No Changes in known memory layout.\n");
                return result;
            }

            // Deep scan is required due to changes in memory layout
            printf("Processor: Changes detected to memory layout.\n");
        }

        // Perform deep scan
        printf("Processor: Performing deep pattern scan\n");

        // Initialize API structure for deep scan
        t_API api = {m_list, std::vector<size_t>(m_list.size(), 0), result};
        // Call the processor with our ProcessByte function as callback
        t_ProcessorAPI* processorAPI = reinterpret_cast<t_ProcessorAPI*>(processorData);
        processor(processorAPI, &api, reinterpret_cast<t_Callback>(ProcessByte));
        for (size_t i = 0; i < m_list.size(); i++) {
            // set foundoffset to the first found offset if any or SIZE_MAX if not found
            m_list[i]->m_FoundOffset = result.foundOffsets[i].size() ? result.foundOffsets[i][0] : SIZE_MAX;
        }

        return result;
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