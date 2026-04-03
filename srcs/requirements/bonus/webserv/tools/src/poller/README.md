
# simple event IO mit identischer API auf Linux, macOS und Windows. reactor design compatible

Architecture
- Namespace: `event`
- Kern-API: `EventPoller` (Interface), `make_poller()` (Factory)
- Backends:
	- Linux: epoll
	- BSD/macOS: kqueue
	- Windows: select (IOCP wäre komplizierrter)
- Socket-Hilfsfunktionen (plattformunabhängige Wrapper):
	- `create_socket()`, `close_socket()`
	- `set_socket_reuse()`, `set_non_blocking()`
	- `accept_connection()`
	- `receive_data()`, `send_data()`

Bauen und Starten
1) Build-Verzeichnis anlegen und konfigurieren
```
mkdir build
cd build
cmake ..
```

1) Kompilieren
```
cmake --build .
```

1) Ausführen
```
# Linux/macOS
./catsurf

# Windows
./Debug/catsurf.exe
```

API-Überblick
Header: `poller.h` (Namespace `event`)
- `struct PollEvent { int fd; bool readable; bool writable; }`
	- Ein einzelnes Bereitschaftsereignis für einen File Descriptor / Socket.
- `class EventPoller`
	- `add(fd, readable, writable)` 
	- `update(fd, readable, writable)`
	- `remove(fd)`
	- `wait(timeout_ms) -> std::vector<PollEvent>`: Blockiert bis Ereignisse/Timeout.
- `std::unique_ptr<EventPoller> make_poller()`
	- Wählt das passende Backend abhängig vom Betriebssystem.
- Socket-Hilfsfunktionen
	- `create_socket()`: Erstellt einen TCP-Socket (inkl. WSAStartup auf Windows).
	- `set_socket_reuse(fd)`: Aktiviert SO_REUSEADDR.
	- `set_non_blocking(fd)`: Schaltet Non-Blocking-Modus ein.
	- `close_socket(fd)`: Schließt den Socket plattformrichtig.
	- `accept_connection(listen_fd)`: Non-blockierendes `accept` (liefert -1 wenn „würde blockieren“).
	- `receive_data(fd, buf, size)`: Non-blockierendes `recv` (0 bei „keine Daten“, -1 bei Fehler).
	- `send_data(fd, data, size)`: Non-blockierendes `send` (0 bei „würde blockieren“, -1 bei Fehler).

Benutzung – einfacher Server-Loop (Beschreibung)
- Listening-Socket anlegen: `create_socket()`, `set_socket_reuse()`, `bind+listen`, `set_non_blocking()`
- Poller erstellen: `auto poller = event::make_poller();`
- Listening-FD anmelden: `poller->add(listen_fd, /*readable=*/true, /*writable=*/false);`
- Event-Loop:
	- `for (auto& ev : poller->wait(1000))` durchlaufen
	- Wenn `ev.fd == listen_fd` und `ev.readable`: so lange `accept_connection()` aufrufen, bis -1 zurückkommt; neue Clients als readable registrieren
	- Wenn `ev.readable` (Client): mit `receive_data()` lesen; bei <=0 Verbindung entfernen und schließen; sonst Antwort mit `send_data()` schreiben und schließen (Beispiel in `main.cpp`)
