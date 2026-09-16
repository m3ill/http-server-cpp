# HTTP Server MVP

C++20 ve POSIX socket API ile yazılmış, tek istemcili küçük bir HTTP/1.1 server ve ona eşlik eden HTTP client örneği.

Bu proje TCP'nin yalnızca byte stream sağladığını; HTTP'nin ise bu byte'ları request line, header ve response kurallarıyla yorumladığını göstermek için tasarlanmıştır.

## Özellikler

- `127.0.0.1:8080` üzerinde dinleyen tek istemcili TCP server
- `\r\n\r\n` görülene kadar header byte'larını biriktiren `readHeaders()`
- En fazla 8 KiB request header sınırı
- `GET /health` ve `GET /hello` routing'i
- `400 Bad Request`, `404 Not Found` ve `405 Method Not Allowed` yanıtları
- Doğru `Content-Length`, `Content-Type` ve `Connection: close` header'ları
- Partial `send()` için `sendAll()`
- `GET /health` gönderen ve response'u EOF'a kadar okuyan örnek client

## Proje yapısı

```text
HTTPServer/
├── main.cpp         # HTTP server
├── client.cpp       # Örnek HTTP client
├── CMakeLists.txt
└── README.md
```

## Gereksinimler

- Linux veya WSL
- C++20 destekleyen bir derleyici
- CMake 4.4 veya üzeri
- Raw testler için `curl` ve `nc` (netcat)

## Derleme

Proje dizininde:

```bash
cmake -S . -B build
cmake --build build
```

İki executable oluşur:

```text
build/HTTPServer
build/HTTPClient
```

## Çalıştırma

Server tek bağlantı kabul eder ve response gönderildikten sonra kapanır. Bir terminalde server'ı başlat:

```bash
./build/HTTPServer
```

Başka bir terminalde örnek client'ı çalıştır:

```bash
./build/HTTPClient
```

Client aşağıdaki raw response'u yazdırır:

```http
HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 2
Connection: close

OK
```

## Endpoint'ler

| Request | Response | Body |
|---|---|---|
| `GET /health` | `200 OK` | `OK` |
| `GET /hello` | `200 OK` | `Merhaba HTTP` |
| Bilinmeyen `GET` path | `404 Not Found` | `Not Found` |
| `GET` dışı method | `405 Method Not Allowed` | `Method Not Allowed` |
| Bozuk request line | `400 Bad Request` | `Bad Request` |

Her istek için server'ı yeniden başlatıp aşağıdaki komutlardan birini çalıştır:

```bash
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/hello
curl -i http://127.0.0.1:8080/nope
curl -i -X POST http://127.0.0.1:8080/hello
```

## Request/response akışı

```text
client
  → TCP byte stream
  → readHeaders()          # \r\n\r\n görülene kadar biriktirir
  → parseRequest()         # method, path, HTTP version
  → routeRequest()         # 200 / 404 / 405 seçer
  → buildResponse()
  → sendAll()
  → client
```

Örnek request:

```http
GET /hello HTTP/1.1
Host: 127.0.0.1:8080
Connection: close

```

## Raw testler

### Bozuk request line

Server çalışırken bu request, HTTP version içermediği için `400 Bad Request` döndürür:

```bash
printf 'GET /hello\r\nHost: 127.0.0.1:8080\r\n\r\n' | nc -N 127.0.0.1 8080
```

### Partial receive

Bu komut tek bir HTTP request'ini iki farklı yazıma böler. `readHeaders()` her iki parçayı buffer'da birleştirir ve `200 OK` döndürür:

```bash
{
  printf 'GET /hello HTTP/1.1\r\nHost: 127.'
  sleep 0.2
  printf '0.0.1:8080\r\nConnection: close\r\n\r\n'
} | nc -N 127.0.0.1 8080
```

## Bilinen sınırlar

- Tek client ve tek request desteklenir; response'tan sonra bağlantı kapanır.
- Yalnızca request line parse edilir; header değerleri kullanılmaz.
- Sadece `GET` route edilir; POST body, request body ve chunked encoding desteklenmez.
- Keep-alive, pipelining, TLS, static dosya sunumu ve concurrency kapsam dışıdır.
- Header okuma aşamasında client bağlantıyı erken kapatırsa veya header 8 KiB'ı aşarsa server response göndermeden bağlantıyı kapatır.

## Doğrulama

WSL üzerinde aşağıdaki senaryolar doğrulandı:

- Örnek client ile `GET /health` → `200 OK`, `OK`
- `curl` ile `/health`, `/hello`, bilinmeyen rota ve `POST /hello`
- Netcat ile bozuk request line → `400 Bad Request`
- Netcat ile iki parçalı `GET /hello` → `200 OK`
