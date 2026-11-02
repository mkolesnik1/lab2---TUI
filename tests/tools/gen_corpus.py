#!/usr/bin/env python3
"""Генератор тестового ящика Maildir (tests/data/maildir) и таблицы ожидаемых результатов (expected.tsv).

Письма собраны вручную из байтов — как они выглядят «на проводе»; библиотеки разбора почты не используются.
Запуск: python3 tests/tools/gen_corpus.py   (результат закоммичен, перегенерировать нужно только при правках).
"""
import base64
import datetime
import pathlib
import quopri
import shutil

DATA = pathlib.Path(__file__).resolve().parent.parent / "data"
ROOT = DATA / "maildir"

# ---------- помощники ----------


def hdr(headers):
    return "".join(f"{k}: {v}\n" for k, v in headers).encode("utf-8")


def part(headers, body):
    return hdr(headers) + b"\n" + body


def b64(data):
    s = base64.b64encode(data).decode()
    return ("\n".join(s[i:i + 76] for i in range(0, len(s), 76)) + "\n").encode()


def qp(text, cs):
    return quopri.encodestring(text.encode(cs))


def ew_b(text, cs):
    return f"=?{cs}?B?{base64.b64encode(text.encode(cs)).decode()}?="


def ew_q(text, cs):
    q = quopri.encodestring(text.encode(cs), quotetabs=True, header=True).decode().replace("\n", "")
    return f"=?{cs}?Q?{q}?="


def multipart(sub, boundary, parts, close=True):
    body = b""
    for p in parts:
        body += f"--{boundary}\n".encode() + p + b"\n"
    if close:
        body += f"--{boundary}--\n".encode()
    return f'multipart/{sub}; boundary="{boundary}"', body


def text_part(ctype, cs, body, cte="7bit"):
    return part([("Content-Type", f"{ctype}; charset={cs}"), ("Content-Transfer-Encoding", cte)], body)


def att_part(mime, filename_param, data, disp="attachment"):
    return part([("Content-Type", f"{mime}"),
                 ("Content-Disposition", f"{disp}; {filename_param}"),
                 ("Content-Transfer-Encoding", "base64")], b64(data))



# ---------- тексты писем ----------
# Первая строка каждого письма — фрагмент из expected.tsv (по нему тесты проверяют тело), дальше — «живой» текст,
# чтобы в клиенте было что читать и прокручивать. Письма-ловушки (битый base64, multipart без boundary и т.п.) не трогаем.

T = {}

T[1] = """Hello, this is a plain ASCII message.

Hi!

I am writing to confirm that the quarterly planning meeting is moved to Thursday, 15:00, room 304.
Please bring your notes on the open items from the last sprint:

  1. Finish the migration of the old reports to the new format.
  2. Review the pull requests that have been waiting for more than a week.
  3. Update the user documentation before the release.

If Thursday does not work for you, reply to this message and we will find another time.

Best regards,
Alice Smith
"""

T[2] = """Привет, мир! Это письмо в UTF-8.

Борис пишет из поездки. Сначала коротко о главном, потом подробности.

1. Конференция прошла хорошо: доклад поставили в первый блок, зал был почти полный, вопросов задали больше, чем мы ожидали.
2. Билеты на обратную дорогу уже куплены, вернусь в воскресенье вечером.
3. Ключи от кабинета оставил у вахтёра, если понадобятся материалы по проекту.

Теперь подробнее. До города мы добирались почти сутки: сначала самолёт, потом долгая пересадка, потом ещё три часа на автобусе по горной дороге. Зато вид из окна того стоил: озеро, облака внизу, на склонах уже лежит первый снег. В гостинице тихо, завтрак (каша, сырники, крепкий чай) подают с семи утра, а Wi-Fi работает только в холле, поэтому на письма отвечаю в основном вечером.

Что успел сделать по работе:
  - собрал замечания к статье и внёс правки в разделы 2 и 4;
  - договорился с коллегами из соседней лаборатории об общих данных;
  - переписал скрипт обработки так, чтобы он не падал на пустых строках;
  - набросал план на следующий квартал (пришлю на неделе).

Что хочу обсудить, когда вернусь:
  - как лучше разделить задачи между тремя людьми, чтобы никто не ждал остальных;
  - какие документы стоит хранить локально, а какие можно забрать из архива;
  - нужен ли нам отдельный человек на тестирование или справимся своими силами.

Если будет время, посмотрите, пожалуйста, мой черновик в общей папке и напишите, что непонятно. Я не обижусь на жёсткую критику, мне важнее, чтобы текст читался.

Это письмо специально сделано длинным: строк в нём больше, чем помещается на экране терминала, поэтому на нём удобно проверять прокрутку: стрелки ↑ и ↓, Space, PageUp и PageDown, Home и End.

Пока всё. До встречи!

Борис

P.S. Привезу местный мёд и сушёные яблоки, если вы их любите.
"""

T[3] = """Файл с отчётом готов. Спасибо!

Коллеги, добрый день.

Итоги октября в двух словах:
  - план по новым клиентам выполнен на 112%;
  - среднее время ответа поддержки сократилось с 6 часов до 2,5;
  - расходы на рекламу не вышли за бюджет, экономия составила около 8%;
  - две задачи из квартального плана перенесены на ноябрь (см. раздел 4).

Подробные таблицы и графики лежат в общей папке «Отчёты». Если заметите расхождения со своими данными, напишите мне до пятницы, и я успею поправить отчёт до отправки руководству.

С уважением,
Анна Петрова
"""

T[4] = """Это письмо в кодировке KOI8-R, передано как quoted-printable.

Раньше KOI8-R была самой распространённой кодировкой русской почты, особенно в системах семейства Unix. Сегодня почти все пишут в UTF-8, но старые письма и архивы рассылок до сих пор лежат в KOI8-R, поэтому почтовый клиент обязан их читать.

Если вы видите это сообщение нормальным русским текстом, а не набором странных символов, значит, перекодировка работает правильно.

Иван
"""

T[5] = """Договор подписан, документы во вложении.

Добрый день!

Договор №5 подписан обеими сторонами. Оригиналы отправлены курьером, срок доставки — до двух рабочих дней. Сканы первой и последней страниц оставил в общей папке для вашего архива.

Напоминаю основные условия:
  - срок действия: 12 месяцев с даты подписания;
  - оплата: в течение 10 банковских дней после получения счёта;
  - приёмка работ: по актам, раз в месяц.

Если понадобятся изменения или дополнительные соглашения, пишите.

С уважением,
Сергей
"""

T[6] = """Un café, un résumé, naïve.

Bonjour,

Voici le menu de la semaine au café de la gare :

  Lundi      soupe à l'oignon, tarte aux pommes
  Mardi      salade niçoise, crème brûlée
  Mercredi   quiche lorraine, mousse au chocolat
  Jeudi      poulet rôti, pommes de terre sautées, fromage
  Vendredi   poisson du jour, tarte au citron

Le café est servi à toute heure. Réservation conseillée pour le déjeuner, surtout le vendredi.

À bientôt,
Le chef
"""

T[7] = """Zażółć gęślą jaźń - test ISO-8859-2.

Cześć!

To zdanie zawiera wszystkie polskie znaki diakrytyczne: ą, ć, ę, ł, ń, ó, ś, ź, ż. Jeśli widzisz je poprawnie, to przekodowanie z ISO-8859-2 działa.

Pozdrawiam,
Jan
"""

T[8] = """こんにちは世界

田中です。お元気ですか。

来週の月曜日に、新しいプロジェクトの打ち合わせをしたいと思います。
場所は三階の会議室、時間は午後二時からです。

資料は前日までにお送りします。
ご都合が悪い場合は、お知らせください。

よろしくお願いいたします。
田中
"""

T[9] = """日本語のメール

佐藤です。

このメールは、ISO-2022-JP という文字コードで送られています。昔の日本語のメールでは、この形式がよく使われていました。

文字が正しく読めていれば、変換は成功です。

よろしくお願いします。
佐藤
"""

T[10] = """你好，世界

我是小王。

这是一封用 GB2312 编码的邮件。请确认中文能够正常显示，包括标点符号，比如逗号、句号和引号。

下周三下午两点开会，地点在二楼会议室。请提前阅读会议资料，并准备好自己的意见。

谢谢！
小王
"""

T[11] = """哈囉世界

我是小陳。

這封信使用 Big5 編碼，是臺灣常見的舊式中文編碼。如果你能正常看到這些字，表示轉碼沒有問題。

下週三下午兩點開會，地點在二樓會議室，請提前閱讀會議資料，並準備好意見。

謝謝！
小陳
"""

T[12] = ("<h2>Новости недели</h2>"
         "<p>На этой неделе вышла новая версия сайта: главная страница стала открываться быстрее, а поиск теперь понимает опечатки.</p>"
         "<p>Что ещё нового:</p>"
         "<ul><li>раздел с ответами на частые вопросы;</li><li>тёмная тема оформления;</li><li>рассылка стала приходить раз в неделю, а не каждый день.</li></ul>"
         "<p>Спасибо, что читаете нас!</p>")

T[13] = ("<p>Скидка действует на всю зимнюю коллекцию: куртки, шарфы, шапки и перчатки.</p>"
         "<ul><li>Бесплатная доставка при заказе от 3000 рублей.</li><li>Возврат в течение 14 дней.</li>"
         "<li>Скидки не суммируются с другими акциями.</li></ul>"
         "<p>Спасибо, что выбираете наш магазин!</p>\n")

T[14] = """Plain version wins

This is the plain-text part of a multipart/alternative message. A client that prefers plain text shows this part and ignores the HTML version.
"""

T[15] = """Plain after HTML

Here the HTML part comes first and the plain-text part second. The client still picks the plain text, because it is the last (and therefore the most faithful) alternative.
"""

T[16] = """Документ во вложении.

Добрый день! Направляю на согласование проект договора (файл contract.pdf). Прошу посмотреть разделы 3 и 5 и вернуть замечания до конца недели.

Юридический отдел
"""

T[17] = """Two files.

Hi,

attached are the report and the photo from the site visit. The report is a plain text file, the photo is a PNG. Please have a look before the meeting.

Thanks,
Boss
"""

T[18] = """RFC 2231 filename.

This message has an attachment whose file name is written in the RFC 2231 form (filename*=utf-8''...). A correct client shows the name as an ordinary Russian word, not as a string of percent signs.
"""

T[19] = """Continuation filename.

Here the long file name is split into several pieces (filename*0*, filename*1*, ...). The client must glue them together and decode the result.
"""

T[20] = """Главный текст письма

Это основной текст письма. Внутри есть HTML-вариант с картинкой и вложение archive.zip; дерево частей можно посмотреть клавишей p.
"""

T[21] = """Three levels deep

This text sits three multipart levels deep: mixed, then mixed again, then alternative. The client must dig down to it and show it as the body of the message.
"""

T[22] = """See forwarded message below.

Hi, forwarding the original message from Orig. Please read it and tell me what you think. The forwarded letter is attached as a part of type message/rfc822: open the parts tree with p to see it.
"""

T[22, 'inner'] = """Текст пересланного письма

Это текст исходного письма, которое переслали вложением. В списке частей оно называется «Исходное письмо».
"""

T[23] = """Level 2 text

The second-level message, forwarded inside another message, which itself contains a third one.
"""

T[24] = """Delivery failed: mailbox unavailable

This is the mail system at host mx.example.com.

I'm sorry to have to inform you that your message could not be delivered to one or more recipients. The details are in the parts below.

<nobody@example.com>: host mail.example.com said: 550 5.1.1 User unknown
"""

T[25] = """Old style message without MIME headers.

There is no MIME-Version or Content-Type header in this message. Letters like this were common before MIME appeared, and a client should show them as plain ASCII text.
"""

T[26] = """
The charset in the header is unknown (x-unknown-9), so the client has to guess. The first line contains a single byte 0xE9, which looks like the letter e with an acute accent in Latin-1.
"""

T[29] = """Truncated message text

This letter was cut off: the closing boundary line is missing. The client must still show everything that has arrived.
"""

T[30] = ("\r\nThis message uses Windows line endings (CRLF).\r\nThe client must not show stray carriage return characters "
         "at the end of the lines.\r\n")

T[31] = """Многобайтный символ разрезан между словами

Тема этого письма записана двумя закодированными словами, и граница между ними проходит посреди многобайтного символа. Клиент должен сначала склеить байты и только потом декодировать их, тогда в списке будет тема «Привет мир».
"""

T[32] = """Root of thread

Hi all,

here is the first draft of the project plan:

  1. Week 1-2: collect requirements and agree on the scope.
  2. Week 3-5: build the prototype.
  3. Week 6: testing and bug fixing.
  4. Week 7: release and retrospective.

Please reply with your comments. Bob, Carol and Dave, I would like to hear from each of you.

Alice
"""

T[33] = """Reply B

Hi Alice,

the plan looks reasonable, but I think two weeks for requirements is too short. Last time it took us three.

> 1. Week 1-2: collect requirements and agree on the scope.

Can we move the prototype one week later?

Bob
"""

T[34] = """Reply C to B

Bob, I agree about the requirements. But if we move the prototype, testing must not shrink: it was the part we cut last time and it hurt.

> Can we move the prototype one week later?

Maybe we can start the testing plan in parallel with the prototype.

Carol
"""

T[35] = """Reply D to root

Alice, thanks for the plan. I can take the release part (week 7): prepare the checklist and the release notes. I will also book the room for the retrospective.

Dave
"""

T[36] = """Parent is not in the mailbox

Hi,

I am replying to a message that is not in this mailbox: it may have been deleted or never delivered. The client should still show this letter, just without a parent in the thread tree.

Eve
"""

T[37] = """Cycle X body

This letter says it is a reply to Cycle Y, and Cycle Y says it is a reply to this one. Such a loop is a broken header, and the client must not hang on it.
"""

T[38] = """Cycle Y body

See the letter Cycle X: the two messages point at each other.
"""

T[40] = """No subject here

This message has neither a Subject nor a From header. In the list it appears as (без темы) with an empty sender.
"""

T[42] = ("<p>HTML body wins</p><p>The message has an HTML body and a text attachment (notes.txt). The client shows the HTML "
         "as plain text; the attachment can be found in the parts tree (key p).</p>\n")

T[43] = """
This line comes after a soft line break: a quoted-printable equals sign at the end of a line glues it to the next one.
"""

T[44] = """Привет без метки charset

Это письмо написано в UTF-8, но в заголовке указано us-ascii. Клиент должен заметить байты вне ASCII и показать русский текст, а не вопросительные знаки.
"""

T[45] = """Date with zone comment.

У этого письма три флага: помечено (F), отвечено (R) и прочитано (S). В списке они видны слева от даты.
"""

T[46] = """Name with a comma.

The sender name is written as "Doe, John" in quotes. A naive parser would split it at the comma into two addresses; the client must keep it as one name.
"""

T[47] = """Encoded word inside quotes.

The sender name here is an encoded word (RFC 2047) wrapped in quotes. The client shows it as Иван Иванов. The letter is in the trash (flag T).
"""

T[48] = """Header names are case-insensitive.

Header names in this letter are written like fRoM and subJECT; the client must understand them anyway.
"""

# ---------- письма ----------

mails = []  # (папка, флаги, байты, ожидания)


def date_for(n, hour=10):
    d = datetime.datetime(2026, 10, 1, hour, 0, 0) + datetime.timedelta(days=n)
    return d.strftime("%a, %d %b %Y %H:%M:%S +0700")


def add(num, flags, subject, frm, extra, body, *, ctype=None, cte=None, expect_subject="", expect_from="",
        snippet="", attachments="", folder="cur", message_id=True, date=True, raw_headers=None, mime_version=True):
    headers = []
    if frm is not None:
        headers.append(("From", frm))
    headers.append(("To", "me@example.org"))
    if subject is not None:
        headers.append(("Subject", subject))
    if date:
        headers.append(("Date", date_for(num)))
    if message_id is True:
        headers.append(("Message-ID", f"<m{num:02d}@test>"))
    elif message_id:
        headers.append(("Message-ID", message_id))
    if mime_version:
        headers.append(("MIME-Version", "1.0"))
    headers += extra
    if ctype:
        headers.append(("Content-Type", ctype))
    if cte:
        headers.append(("Content-Transfer-Encoding", cte))
    raw = hdr(headers) + b"\n" + body
    if raw_headers is not None:
        raw = raw_headers + b"\n" + body
    mails.append((folder, flags, num, raw, (expect_subject, expect_from, snippet, attachments)))


# 01-11: кодировки
add(1, "S", "Hello world", "Alice Smith <alice@example.com>", [], T[1].encode(),
    ctype="text/plain; charset=us-ascii", cte="7bit", expect_subject="Hello world", expect_from="Alice Smith",
    snippet="plain ASCII message")
add(2, "S", ew_b("Привет", "utf-8"), "Борис <boris@example.ru>", [], T[2].encode(),
    ctype="text/plain; charset=utf-8", cte="8bit", expect_subject="Привет", expect_from="Борис",
    snippet="Привет, мир! Это письмо в UTF-8.")
add(3, "FS", ew_b("Отчёт за октябрь", "utf-8"), f"{ew_b('Анна Петрова', 'utf-8')} <anna@example.ru>", [],
    b64(T[3].encode()),
    ctype="text/plain; charset=utf-8", cte="base64", expect_subject="Отчёт за октябрь", expect_from="Анна Петрова",
    snippet="Файл с отчётом готов. Спасибо!")
add(4, "S", ew_q("Привет из KOI8-R", "koi8-r"), f"{ew_b('Иван Петров', 'koi8-r')} <ivan@example.ru>", [],
    qp(T[4], "koi8-r"),
    ctype="text/plain; charset=koi8-r", cte="quoted-printable", expect_subject="Привет из KOI8-R",
    expect_from="Иван Петров", snippet="кодировке KOI8-R, передано как quoted-printable")
add(5, "RS", ew_b("Договор №5", "windows-1251"), "Сергей <sergey@example.ru>", [],
    b64(T[5].encode("cp1251")),
    ctype="text/plain; charset=windows-1251", cte="base64", expect_subject="Договор №5", expect_from="Сергей",
    snippet="Договор подписан, документы во вложении.")
add(6, "S", ew_q("Café menu", "iso-8859-1"), "Chef <chef@example.fr>", [], qp(T[6], "iso-8859-1"),
    ctype="text/plain; charset=iso-8859-1", cte="quoted-printable", expect_subject="Café menu", expect_from="Chef",
    snippet="Un café, un résumé, naïve.")
add(7, "S", ew_b("Zażółć gęślą", "iso-8859-2"), "Jan Kowalski <jan@example.pl>", [],
    T[7].encode("iso-8859-2"),
    ctype="text/plain; charset=iso-8859-2", cte="8bit", expect_subject="Zażółć gęślą", expect_from="Jan Kowalski",
    snippet="Zażółć gęślą jaźń - test ISO-8859-2.")
add(8, "S", ew_b("こんにちは", "shift_jis"), f"{ew_b('田中', 'shift_jis')} <tanaka@example.jp>", [],
    b64(T[8].encode("shift_jis")), ctype="text/plain; charset=shift_jis", cte="base64",
    expect_subject="こんにちは", expect_from="田中", snippet="こんにちは世界")
add(9, "S", ew_b("日本語", "iso-2022-jp"), "Sato <sato@example.jp>", [], T[9].encode("iso-2022-jp"),
    ctype="text/plain; charset=iso-2022-jp", cte="7bit", expect_subject="日本語", expect_from="Sato",
    snippet="日本語のメール")
add(10, "S", ew_b("你好", "gb2312"), "Wang <wang@example.cn>", [], b64(T[10].encode("gb2312")),
    ctype="text/plain; charset=gb2312", cte="base64", expect_subject="你好", expect_from="Wang", snippet="你好，世界")
add(11, "S", ew_b("哈囉", "big5"), "Chen <chen@example.tw>", [], qp(T[11], "big5"),
    ctype="text/plain; charset=big5", cte="quoted-printable", expect_subject="哈囉", expect_from="Chen",
    snippet="哈囉世界")

# 12-13: только HTML
html12 = ("<html><head><title>Не показывать</title><style>p{color:red}</style></head><body><h1>Заголовок</h1>"
          "<p>Привет, <b>мир</b> &amp; друзья&nbsp;!</p><ul><li>раз</li><li>два</li></ul>"
          + T[12] + "<script>alert(1)</script></body></html>\n")
add(12, "", "HTML only", "News <news@example.com>", [], html12.encode(), ctype="text/html; charset=utf-8", cte="8bit",
    expect_subject="HTML only", expect_from="News", snippet="Привет, мир & друзья", folder="new")
add(13, "S", "HTML cp1251", "Shop <shop@example.ru>", [],
    qp("<p>Скидка &laquo;50%&raquo; &#1076;о конца <i>недели</i></p>" + T[13], "windows-1251"),
    ctype="text/html; charset=windows-1251", cte="quoted-printable", expect_subject="HTML cp1251", expect_from="Shop",
    snippet="Скидка «50%» до конца недели")

# 14-15: alternative
ct, body = multipart("alternative", "ALT14", [
    text_part("text/plain", "utf-8", T[14].encode()),
    text_part("text/html", "utf-8", b"<p>HTML version</p><p>The same text, but as HTML.</p>\n")])
add(14, "S", "Alternative", "Dev <dev@example.com>", [], body, ctype=ct, expect_subject="Alternative", expect_from="Dev",
    snippet="Plain version wins")
ct, body = multipart("alternative", "ALT15", [
    text_part("text/html", "utf-8", b"<p>HTML first</p><p>The same text, but as HTML.</p>\n"),
    text_part("text/plain", "utf-8", T[15].encode())])
add(15, "S", "Alternative reversed", "Dev <dev@example.com>", [], body, ctype=ct, expect_subject="Alternative reversed",
    expect_from="Dev", snippet="Plain after HTML")

# 16-19: вложения
ct, body = multipart("mixed", "MIX16", [
    text_part("text/plain", "utf-8", T[16].encode(), "8bit"),
    att_part("application/pdf", 'filename="contract.pdf"', b"%PDF-1.4\n" + bytes(range(256)) * 4)])
add(16, "S", "PDF attached", "Legal <legal@example.com>", [], body, ctype=ct, expect_subject="PDF attached",
    expect_from="Legal", snippet="Документ во вложении.", attachments="contract.pdf")
ct, body = multipart("mixed", "MIX17", [
    text_part("text/plain", "utf-8", T[17].encode()),
    att_part("text/plain", f'filename="{ew_b("отчёт.txt", "utf-8")}"', "Содержимое отчёта\n".encode()),
    att_part("image/png", f'filename="{ew_q("фото.png", "utf-8")}"', b"\x89PNG\r\n\x1a\n" + bytes(range(64)))])
add(17, "FS", "Two attachments", "Boss <boss@example.com>", [], body, ctype=ct, expect_subject="Two attachments",
    expect_from="Boss", snippet="Two files.", attachments="отчёт.txt|фото.png")
ct, body = multipart("mixed", "MIX18", [
    text_part("text/plain", "utf-8", T[18].encode()),
    att_part("application/pdf", "filename*=utf-8''%D0%B4%D0%BE%D0%BA%D1%83%D0%BC%D0%B5%D0%BD%D1%82.pdf", b"%PDF-1.4 x")])
add(18, "S", "RFC2231 name", "Gmail <g@example.com>", [], body, ctype=ct, expect_subject="RFC2231 name",
    expect_from="Gmail", snippet="RFC 2231 filename.", attachments="документ.pdf")
ct, body = multipart("mixed", "MIX19", [
    text_part("text/plain", "utf-8", T[19].encode()),
    att_part("application/pdf",
             "filename*0*=utf-8''%D0%BE%D1%87%D0%B5%D0%BD%D1%8C%D0%94%D0%BB%D0%B8%D0%BD;\n filename*1*=%D0%BD%D0%BE%D0%B5.pdf",
             b"%PDF-1.4 y")])
add(19, "S", "RFC2231 continuation", "Gmail <g@example.com>", [], body, ctype=ct, expect_subject="RFC2231 continuation",
    expect_from="Gmail", snippet="Continuation filename.", attachments="оченьДлинное.pdf")

# 20-21: вложенный multipart
alt = multipart("alternative", "ALT20", [
    text_part("text/plain", "utf-8", T[20].encode(), "8bit"),
    part([("Content-Type", 'multipart/related; boundary="REL20"')],
         multipart("related", "REL20", [
             text_part("text/html", "utf-8", b'<p>HTML <img src="cid:logo"></p>\n'),
             part([("Content-Type", "image/png"), ("Content-Disposition", "inline; filename=logo.png"),
                   ("Content-ID", "<logo>"), ("Content-Transfer-Encoding", "base64")], b64(b"\x89PNG" + bytes(40)))])[1])])
ct, body = multipart("mixed", "MIX20", [
    part([("Content-Type", alt[0])], alt[1]),
    att_part("application/zip", 'filename="archive.zip"', b"PK\x03\x04" + bytes(100))])
add(20, "S", "Deeply nested", "Team <team@example.com>", [], body, ctype=ct, expect_subject="Deeply nested",
    expect_from="Team", snippet="Главный текст письма", attachments="archive.zip")
inner = multipart("alternative", "ALT21", [text_part("text/plain", "utf-8", T[21].encode()),
                                           text_part("text/html", "utf-8", b"<p>deep html</p>\n")])
mid = multipart("mixed", "MIX21B", [part([("Content-Type", inner[0])], inner[1])])
ct, body = multipart("mixed", "MIX21A", [part([("Content-Type", mid[0])], mid[1])])
add(21, "S", "Three levels", "Team <team@example.com>", [], body, ctype=ct, expect_subject="Three levels",
    expect_from="Team", snippet="Three levels deep")

# 22-24: message/rfc822, report
fwd_inner_alt = multipart("alternative", "ALT22", [text_part("text/plain", "utf-8", T[22, "inner"].encode(), "8bit"),
                                                   text_part("text/html", "utf-8", b"<p>fwd html</p>\n")])
fwd = part([("From", "Orig <orig@example.com>"), ("Subject", ew_b("Исходное письмо", "utf-8")),
            ("Date", date_for(0)), ("MIME-Version", "1.0"), ("Content-Type", fwd_inner_alt[0])], fwd_inner_alt[1])
ct, body = multipart("mixed", "MIX22", [
    text_part("text/plain", "utf-8", T[22].encode()),
    part([("Content-Type", "message/rfc822"), ("Content-Disposition", "inline")], fwd)])
add(22, "S", "Fwd: forwarded", "Me <me2@example.com>", [], body, ctype=ct, expect_subject="Fwd: forwarded",
    expect_from="Me", snippet="See forwarded message below.")
l3 = part([("From", "L3 <l3@example.com>"), ("Subject", "Level 3"), ("Content-Type", "text/plain; charset=us-ascii")],
          b"Level 3 text\n")
l2m = multipart("mixed", "MIX23B", [text_part("text/plain", "us-ascii", T[23].encode()),
                                    part([("Content-Type", "message/rfc822")], l3)])
l2 = part([("From", "L2 <l2@example.com>"), ("Subject", "Level 2"), ("Content-Type", l2m[0])], l2m[1])
ct, body = multipart("mixed", "MIX23A", [part([("Content-Type", "message/rfc822")], l2)])
add(23, "S", "Only embedded messages", "Fwd <fwd@example.com>", [], body, ctype=ct,
    expect_subject="Only embedded messages", expect_from="Fwd", snippet="Level 2 text")
ct, body = multipart("report", "REP24", [
    text_part("text/plain", "us-ascii", T[24].encode()),
    part([("Content-Type", "message/delivery-status")], b"Reporting-MTA: dns; mx.example.com\nStatus: 5.1.1\n"),
    part([("Content-Type", "text/rfc822-headers")], b"Subject: original\nTo: nobody@example.com\n")])
add(24, "S", "Undelivered Mail Returned to Sender", "MAILER-DAEMON <mailer-daemon@example.com>", [], body,
    ctype=ct + "; report-type=delivery-status", expect_subject="Undelivered Mail Returned to Sender",
    expect_from="MAILER-DAEMON", snippet="Delivery failed: mailbox unavailable")

# 25-31: нестандартные и битые письма
add(25, "", "Old style", "Old <old@example.com>", [], T[25].encode(), folder="new",
    mime_version=False, expect_subject="Old style", expect_from="Old", snippet="Old style message without MIME headers.")
add(26, "S", "Unknown charset", "X <x@example.com>", [], b"caf\xe9 au lait\n" + T[26].encode(), ctype="text/plain; charset=x-unknown-9",
    cte="8bit", expect_subject="Unknown charset", expect_from="X", snippet="café au lait")
add(27, "S", "Multipart without boundary", "Broken <broken@example.com>", [], b"Broken multipart body text\n",
    ctype="multipart/mixed", expect_subject="Multipart without boundary", expect_from="Broken",
    snippet="(нет текстового содержимого)")
add(28, "S", "Garbage in base64", "Bad <bad@example.com>", [], b"SGVsbG8s IHdv@@cmxkIQ==\n!!!\n",
    ctype="text/plain; charset=us-ascii", cte="base64", expect_subject="Garbage in base64", expect_from="Bad",
    snippet="Hello, world!")
ct, body = multipart("mixed", "MIX29", [text_part("text/plain", "us-ascii", T[29].encode())], close=False)
add(29, "S", "Missing closing boundary", "Cut <cut@example.com>", [], body, ctype=ct,
    expect_subject="Missing closing boundary", expect_from="Cut", snippet="Truncated message text")
crlf = (b"From: Win <win@example.com>\r\nTo: me@example.org\r\nSubject: CRLF message\r\nDate: " +
        date_for(30).encode() + b"\r\nMessage-ID: <m30@test>\r\nContent-Type: text/plain; charset=us-ascii\r\n\r\n"
        b"CRLF line one\r\nline two\r\n" + T[30].encode())
mails.append(("cur", "S", 30, crlf, ("CRLF message", "Win", "CRLF line one", "")))
pb = "Привет мир".encode()
subj31 = "=?utf-8?B?" + base64.b64encode(pb[:3]).decode() + "?=\n =?utf-8?B?" + base64.b64encode(pb[3:]).decode() + "?="
add(31, "S", subj31, "Fold <fold@example.com>", [], T[31].encode(),
    ctype="text/plain; charset=utf-8", cte="8bit", expect_subject="Привет мир", expect_from="Fold",
    snippet="разрезан между словами")

# 32-38: треды
def thread_mail(num, mid, subject, frm, refs=None, reply=None, flags="S", folder="cur", snippet=""):
    extra = []
    if reply:
        extra.append(("In-Reply-To", f"<{reply}>"))
    if refs:
        extra.append(("References", " ".join(f"<{r}>" for r in refs)))
    add(num, flags, subject, frm, extra, T[num].encode(), ctype="text/plain; charset=us-ascii",
        expect_subject=subject, expect_from=frm.split(" <")[0], snippet=snippet, message_id=f"<{mid}>",
        folder=folder)


thread_mail(32, "root@t", "Project plan", "Alice <alice@example.com>", snippet="Root of thread")
thread_mail(33, "b@t", "Re: Project plan", "Bob <bob@example.com>", refs=["root@t"], reply="root@t", folder="new",
            flags="", snippet="Reply B")
thread_mail(34, "c@t", "Re: Project plan", "Carol <carol@example.com>", refs=["root@t", "b@t"], reply="b@t",
            folder="new", flags="", snippet="Reply C to B")
thread_mail(35, "d@t", "Re: Project plan", "Dave <dave@example.com>", refs=["root@t"], reply="root@t", folder="new",
            flags="", snippet="Reply D to root")
thread_mail(36, "e@t", "Re: Lost thread", "Eve <eve@example.com>", refs=["missing@t"], reply="missing@t",
            snippet="Parent is not in the mailbox")
thread_mail(37, "x@t", "Cycle X", "Xavier <x@example.com>", reply="y@t", snippet="Cycle X body")
thread_mail(38, "y@t", "Cycle Y", "Yvonne <y@example.com>", reply="x@t", snippet="Cycle Y body")

# 39-48: прочее
add(39, "S", "Empty body", "Empty <empty@example.com>", [], b"", ctype="text/plain; charset=us-ascii",
    expect_subject="Empty body", expect_from="Empty", snippet="")
mails.append(("new", "", 40, (f"Date: {date_for(40)}\nContent-Type: text/plain\n\n" + T[40]).encode(),
              ("", "", "No subject here", "")))
ct, body = multipart("mixed", "MIX41", [att_part("application/pdf", 'filename="report.pdf"', b"%PDF-1.4 z")])
add(41, "S", "Attachment only", "Files <files@example.com>", [], body, ctype=ct, expect_subject="Attachment only",
    expect_from="Files", snippet="(нет текстового содержимого)", attachments="report.pdf")
ct, body = multipart("mixed", "MIX42", [
    text_part("text/html", "utf-8", T[42].encode()),
    att_part("text/plain", 'filename="notes.txt"', b"attached notes\n")])
add(42, "S", "Text attachment and HTML body", "Dev <dev@example.com>", [], body, ctype=ct,
    expect_subject="Text attachment and HTML body", expect_from="Dev", snippet="HTML body wins", attachments="notes.txt")
add(43, "S", "QP soft breaks", "QP <qp@example.com>", [],
    b"Line with soft br=\neak and equals =3D sign, caf=C3=A9\n" + qp(T[43], "utf-8"), ctype="text/plain; charset=utf-8",
    cte="quoted-printable", expect_subject="QP soft breaks", expect_from="QP",
    snippet="Line with soft break and equals = sign, café")
add(44, "", "Mislabeled us-ascii", "Label <label@example.com>", [], T[44].encode(),
    ctype="text/plain; charset=us-ascii", cte="8bit", folder="new", expect_subject="Mislabeled us-ascii",
    expect_from="Label", snippet="Привет без метки charset")
add(45, "FRS", "Date with comment", "Date <date@example.com>", [], T[45].encode(),
    ctype="text/plain; charset=us-ascii", expect_subject="Date with comment", expect_from="Date",
    snippet="Date with zone comment.")
add(46, "S", "Quoted name", '"Doe, John" <john@example.com>', [], T[46].encode(),
    ctype="text/plain; charset=us-ascii", expect_subject="Quoted name", expect_from="Doe, John",
    snippet="Name with a comma.")
add(47, "T", "Encoded name in quotes", f'"{ew_b("Иван Иванов", "utf-8")}" <ivanov@example.ru>', [],
    T[47].encode(), ctype="text/plain; charset=us-ascii", expect_subject="Encoded name in quotes",
    expect_from="Иван Иванов", snippet="Encoded word inside quotes.")
mails.append(("cur", "S", 48, (b"fRoM: Case <case@example.com>\nsubJECT: Case test\nDATE: " + date_for(48).encode() +
                               b"\nMessage-ID: <m48@test>\nCONTENT-TYPE: text/plain\n\n" + T[48].encode()),
              ("Case test", "Case", "Header names are case-insensitive.", "")))

# ---------- запись ----------
if ROOT.exists():
    shutil.rmtree(ROOT)
for sub in ("cur", "new", "tmp"):
    (ROOT / sub).mkdir(parents=True)

rows = []
for folder, flags, num, raw, (subj, frm, snippet, atts) in mails:
    unique = f"{1790000000 + num}.M{num}P1.testhost"
    name = unique if folder == "new" else f"{unique}:2,{flags}"
    (ROOT / folder / name).write_bytes(raw)
    rows.append(f"{unique}\t{subj}\t{frm}\t{snippet}\t{atts}")
(DATA / "expected.tsv").write_text(
    "# id\\tsubject\\tfrom\\tbody_contains\\tattachments (| separated)\n" + "\n".join(rows) + "\n", encoding="utf-8")
print(f"{len(mails)} писем -> {ROOT}")
