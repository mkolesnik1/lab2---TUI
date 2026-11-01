#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace mail {

std::string toLowerAscii(std::string_view s);
std::string_view trim(std::string_view s);
bool iequals(std::string_view a, std::string_view b);
bool istartsWith(std::string_view s, std::string_view prefix);

// CRLF и одиночный CR -> LF
std::string normalizeNewlines(std::string_view s);

// UTF-8
std::vector<char32_t> decodeUtf8(std::string_view s);  // битые байты -> U+FFFD
void appendUtf8(std::string& out, char32_t cp);
bool isValidUtf8(std::string_view s);
std::string latin1ToUtf8(std::string_view s);
std::string lowerUtf8(std::string_view s);  // ASCII, Latin-1/Ext-A, кириллица

// Раскладка текста по ячейкам терминала (без зависимости от ncurses и локали)
int cellWidth(char32_t cp);
std::string fitToWidth(std::string_view s, int cols, bool pad);  // обрезка с «…» и добивка пробелами
std::vector<std::string> wrapText(std::string_view text, int cols);

}  // namespace mail
