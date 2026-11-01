# lab2---TUI
Лабораторная работа: TUI приложение на C++ (вариант В - Почтовый клиент)

## maills — читалка локальных почтовых ящиков (Maildir) для терминала

Консольный клиент по образцу mutt: без сети, только чтение локального Maildir. Письма разбираются
вручную (MIME, base64, quoted-printable, RFC 2047), перекодировка — через iconv. Заголовки писем
кэшируются в SQLite, поэтому повторный запуск мгновенный, а новые письма подгружаются в фоне.

## Сборка (Linux)

Нужны компилятор с C++20 (g++ ≥ 11 или clang ≥ 14), CMake ≥ 3.20 и библиотеки разработки SQLite и ncurses:

# Debian / Ubuntu
sudo apt install build-essential cmake libsqlite3-dev libncurses-dev

cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure  # тесты

## Слои программы

Зависимости направлены только вниз, это проверяет тест `layering`:

* `src/core/domain` — предметная область: MIME, base64, quoted-printable, RFC 2047, кодировки, даты, сортировка, фильтры, треды;
* `src/core/application` — сценарии: сессия, команды, клавиши, отрисовка письма в текст;
* `src/infrastructure` — Maildir, индекс SQLite, перекодировка через iconv;
* `src/presentation` — TUI на ncurses.
