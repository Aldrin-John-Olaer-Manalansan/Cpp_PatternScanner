/*
 * @File: PatternScanner.hpp
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

#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <cstddef>

#define PatternScanner_t_Pattern_Initialize(name, FoundOffset, ...) \
	static constexpr short name##_Array[] = __VA_ARGS__; \
	PatternScanner::t_Pattern name(std::span<const short>(name##_Array), FoundOffset);

namespace PatternScanner {
	struct t_ProcessorAPI {
		size_t searchedOffset;
		size_t searchSize;
	};

	using t_Iterator = bool(*)(size_t* outByteIndex, uint8_t* outByte);
	using t_Callback = bool (*)(void* callbackData, size_t byteIndex, uint8_t& byte);
	using t_Processor = void (*)(t_ProcessorAPI* processorData, void* const callbackData, t_Callback callback);

	class t_Pattern {
		std::span<const short> m_AoB;
		size_t m_FoundOffset;

	public:
		struct ScanResult {
			std::vector<size_t> foundOffsets;
			bool isDeepScanned; // useful when the system switches to deepscan when the guessed offset isn't a match
		};
		friend class t_PatternPtrList;

	private:
		struct t_API {
			const std::span<const short> AoB;
			size_t occurenceCount;
			size_t seekIndex;
			ScanResult &result;
		};

		static bool ProcessByte(t_Pattern::t_API *const api, const size_t processedByteIndex, uint8_t& processedByte);

	public:
		static std::vector<short> PatternToAoB(const char* pattern);
		consteval t_Pattern(const std::span<const short> AoB, const size_t foundOffset = SIZE_MAX) : m_AoB(AoB), m_FoundOffset(foundOffset) {}
		t_Pattern(const char* pattern, const size_t foundOffset = SIZE_MAX) : m_FoundOffset(foundOffset) {
			static std::vector<short> storage;
			storage = PatternToAoB(pattern);
			m_AoB = std::span<const short>(storage);
		}
		void PrintInfo(void);
		bool CheckInitialGuessedOffset(const std::span<uint8_t>& data);
		bool CheckInitialGuessedOffset(t_Iterator iterator);
		bool CheckInitialGuessedOffset(void* const processorData, const t_Processor processor);
		ScanResult Scan(const std::span<uint8_t>& data, const bool isDeepScan = false);
		ScanResult Scan(void* const data, const size_t size, const bool isDeepScan = false);
		ScanResult Scan(t_Iterator iterator, const bool isDeepScan = false);
		ScanResult Scan(void* const processorData, const t_Processor processor, const bool isDeepScan = false);
		constexpr size_t GetFoundOffset(void) const {
			return m_FoundOffset;
		}
		void SetFoundOffset(size_t foundOffset) {
			m_FoundOffset = foundOffset;
		}
	};

	class t_PatternPtrList {
		std::span<t_Pattern* const> m_list;
	public:
		struct ScanResult {
			std::vector<std::vector<size_t>> foundOffsets;
			bool isDeepScanned; // useful when the system switches to deepscan when the guessed offset isn't a match
		};

	private:
		struct t_API {
			std::span<t_Pattern* const> list;
			std::vector<size_t> seekIndeces;
			ScanResult &result;
		};

		static bool ProcessByte(t_API *const api, const size_t processedByteIndex, uint8_t& processedByte);

	public:
		consteval t_PatternPtrList(const std::span<t_Pattern* const> list) : m_list(list) {}
		consteval t_PatternPtrList(t_Pattern** const list, const size_t count) {
			m_list = std::span<t_Pattern* const>(list, count);
		}
		constexpr size_t GetCount(void) const {
			return m_list.size();
		}
		std::vector<bool> CheckInitialGuessedOffsets(void* const processorData, const t_Processor processor);
		ScanResult Scan(const std::span<uint8_t> data, const bool isDeepScan = false);
		ScanResult Scan(void* const data, const size_t size, const bool isDeepScan = false);
		ScanResult Scan(t_Iterator iterator, const bool isDeepScan = false);
		ScanResult Scan(void* const processorData, const t_Processor processor, bool isDeepScan = false);
		t_Pattern* operator[](const size_t index) const;
	};
}