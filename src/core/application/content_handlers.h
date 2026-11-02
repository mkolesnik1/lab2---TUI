#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/domain/charset.h"
#include "core/domain/mime.h"

namespace mail {

// Strategy + реестр: как показать часть письма данного MIME-типа.
// priority: чем меньше, тем «предпочтительнее» как тело письма (text/plain=0, text/html=10).
// Новый тип = новый класс + registry.add(...); существующий код не меняется
// (так добавлен text/calendar).
class IContentHandler {
 public:
  virtual ~IContentHandler() = default;
  virtual bool canHandle(const std::string& mimeType) const = 0;
  virtual int priority() const = 0;
  virtual std::string render(const LeafPart& part, const ICharsetConverter& conv) const = 0;
};

class ContentHandlerRegistry {
 public:
  void add(std::unique_ptr<IContentHandler> h) { handlers_.push_back(std::move(h)); }
  const IContentHandler* find(const std::string& mimeType) const;  // nullptr — показать нечем

 private:
  std::vector<std::unique_ptr<IContentHandler>> handlers_;
};

// Декодирование тела: сначала кодировка передачи, потом charset -> UTF-8.
std::string decodeTextBody(const LeafPart& part, const ICharsetConverter& conv);

// Упрощённый текст из HTML: без тегов, script/style, переводы строк для блочных тегов, сущности.
std::string htmlToText(std::string_view utf8Html);

// Приглашение iCalendar (text/calendar) -> читаемый текст: событие, время, место, организатор.
std::string icalToText(std::string_view ics);

void registerBuiltinHandlers(ContentHandlerRegistry& r);  // text/plain, text/html, text/calendar

}  // namespace mail
