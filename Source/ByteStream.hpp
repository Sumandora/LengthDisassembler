#ifndef BYTESTREAM_HPP
#define BYTESTREAM_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

class ByteStream {
	const std::byte* bytes;
	const std::uint8_t length;

	std::uint8_t index = 0;

public:
	ByteStream(const std::byte* bytes, std::uint8_t length)
		: bytes(bytes)
		, length(length)
	{
	}

	std::optional<std::uint8_t> next()
	{
		if (empty())
			return std::nullopt;
		return static_cast<std::uint8_t>(bytes[index++]);
	};

	[[nodiscard]] std::optional<std::uint8_t> peek(std::size_t n = 0) const
	{
		if (!has(n))
			return std::nullopt;
		return static_cast<std::uint8_t>(bytes[index + n]);
	};

	[[nodiscard]] bool has(std::size_t n) const
	{
		return index + n < length;
	}

	[[nodiscard]] bool empty() const
	{
		return index >= length;
	}

	[[nodiscard]] std::uint8_t offset() const
	{
		return index;
	}

	bool consume(std::size_t n)
	{
		index = std::min<std::uint8_t>(index + n, length);
		return !empty();
	}
};

#endif
