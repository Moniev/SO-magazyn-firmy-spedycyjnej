# Magazyn Firmy Spedycyjnej – Raport Projektowy

**Autor:** Robert Moń
**Nr albumu:** MINISTRY-OF-TRUTH-HAS-CENSORED-THIS-LINE
**Repozytorium:** https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej

---

## 1. Założenia i Architektura Systemu

Projekt stanowi symulację działania magazynu firmy spedycyjnej, zaimplementowaną jako
 party na architekturze **Master–Worker/Master–Slave**(ale widzę tu też wzór konsument producent chcoiażby dla Belta i Trucków).
Proces nadrzędny (nasz `Master orchestrator, główny main`) odpowiada za inicjalizację wszystkich zasobów IPC,
uruchamianie procesów potomnych oraz kontrolę ich cyklu życia.

Architektura została zaprojektowana w celu praktycznego zaprezentowania:
- synchronizacji procesów niezależnych,
- klasycznego problemu producent–konsument,
- bezpiecznego współdzielenia pamięci,
- komunikacji asynchronicznej pomiędzy procesami,
- obsługi sygnałów systemowych.

System składa się z wyspecjalizowanych procesów roboczych (*Workerów*),
z których każdy realizuje jasno określoną rolę logiczną w symulowanym środowisku.

### Kluczowe procesy

- **Master (main)**
  Proces nadrzędny inicjalizujący zasoby IPC (SHM, SEM, MSG),
  wykonujący `fork()` i `execv()` w celu uruchomienia workerów.
  Odpowiada za obsługę sygnałów systemowych oraz deterministyczne
  sprzątanie zasobów po zakończeniu symulacji.

- **Belt**
  Proces producenta paczek. Implementuje logiczną taśmę produkcyjną,
  opartą na buforze cyklicznym umieszczonym w pamięci współdzielonej.
  Operacje zapisu są synchronizowane semaforami systemowymi.

- **Dispatcher**
  Konsument paczek. Odpowiada za pobieranie elementów z taśmy (`Belt`)
  oraz koordynację procesu załadunku do ciężarówek.
  Zawiera mechanizmy eliminujące warunki wyścigu.

- **Truck**
  Proces symulujący logistykę pojazdu. Implementuje maszynę stanów,
  reagującą na komunikaty sterujące odbierane z kolejki komunikatów
  (np. sygnał odjazdu, pełny załadunek).

- **Express (VIP)**
  Proces o podwyższonym priorytecie logicznym.
  Generuje paczki uprzywilejowane, które mogą omijać standardową kolejkę
  i trafiać bezpośrednio do załadunku.

- **Terminal**
  Interaktywna konsola operatora (HMI).
  Umożliwia zarządzanie systemem w czasie rzeczywistym
  z wykorzystaniem kontroli dostępu **RBAC** oraz limitów sesji.

---

## 2. Wykorzystane Konstrukcje Systemowe

### A. Tworzenie i zarządzanie procesami

System dynamicznie tworzy procesy potomne z wykorzystaniem
klasycznego modelu `fork()` + `execv()`.
Pozwala to na izolację logiczną komponentów oraz realistyczne
odwzorowanie środowiska wieloprocesowego.

**Funkcje:**
- `fork()`
- `execv()`
- `waitpid()`
- `kill()`

**Kod:**
[src/main.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp) – funkcja `spawnChild`

---

### B. Synchronizacja – Semafory

Semafory System V zostały wykorzystane jako podstawowy mechanizm synchronizacji.
Ich głównym zadaniem jest zapewnienie atomowości operacji na pamięci współdzielonej
oraz zapobieganie race condition.

Semafory realizują:
- problem producent–konsument,
- ochronę sekcji krytycznych,
- synchronizację dostępu do bufora cyklicznego.

**Funkcje:**
- `semget()`
- `semctl()`
- `semop()`

**Kod:**
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h) – metoda `semOperation`

---

### C. Komunikacja – Pamięć Współdzielona

Pamięć współdzielona stanowi centralny magazyn stanu systemu.
Przechowywane są w niej m.in.:
- zawartość taśmy produkcyjnej,
- dane doku załadunkowego,
- struktury sesji użytkowników terminala.

Dostęp do SHM jest chroniony semaforami.

**Funkcje:**
- `shmget()`
- `shmat()`
- `shmdt()`
- `shmctl()`

**Uprawnienia:** `0600`

**Kod:**
[include/Shared.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Shared.h) – struktura `SharedState`
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h) – konstruktor

---

### D. Komunikacja – Kolejki Komunikatów

Kolejki komunikatów System V służą do przesyłania sygnałów sterujących
między procesami w sposób asynchroniczny.

Wykorzystywane są m.in. do:
- wymuszania odjazdu ciężarówek,
- sterowania maszyną stanów procesu `Truck`,
- awaryjnego zatrzymania systemu.

**Funkcje:**
- `msgget()`
- `msgsnd()`
- `msgrcv()`
- `msgctl()`

**Kod:**
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h) – metoda `sendSignal`

---

### E. Obsługa sygnałów

System obsługuje sygnały systemowe w sposób kontrolowany,
zapewniając poprawne zakończenie pracy wszystkich procesów
oraz zwolnienie zasobów IPC.

Obsługiwane sygnały:
- `SIGINT` – bezpieczne zakończenie symulacji,
- `SIGTERM` – zamknięcie procesów potomnych.

**Kod:**
[src/main.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp) – funkcja `handleSigint`

---

## 3. Testy

Projekt charakteryzuje się szerokim pokryciem testami jednostkowymi
oraz integracyjnymi, co znacząco ograniczyło ryzyko wystąpienia
deadlocków i błędów synchronizacji.

### A. Testy jednostkowe i integracyjne

| Moduł | Zakres testów | Plik testowy |
| --- | --- | --- |
| Belt | Bufor cykliczny, FIFO | belt_test.cpp |
| Dispatcher | retry-loop(TOCTOU był wcześniej) | dispatcher_test.cpp |
| Express | Priorytety VIP | express_test.cpp |
| Manager | IPC lifecycle | manager_test.cpp |
| Session | RBAC, quota | session_manager_test.cpp |
| Shared | Integralność SHM | shared_test.cpp |
| Terminal | Symulacja wejścia | terminal_manager_test.cpp |
| Truck | FSM | truck_test.cpp |

---

## 7. Podsumowanie i Wnioski

Projekt umożliwił praktyczne zapoznanie się z mechanizmami IPC,
synchronizacją procesów oraz obsługą sygnałów w systemach UNIX/Linux.
Największym wyzwaniem okazało się wyeliminowanie warunków wyścigu
oraz zapewnienie atomowości operacji na pamięci współdzielonej,
szczególnie w kontekście interaktywnego terminala.

## 8. Automatyzacja Budowania i Zarządzania Projektem (Makefile)

Projekt wykorzystuje rozbudowany plik `Makefile`, który pełni rolę centralnego
narzędzia do zarządzania cyklem życia aplikacji od budowania, przez testowanie,
aż po uruchamianie w środowisku lokalnym i kontenerowym.

Makefile został zaprojektowany jako warstwa abstrakcji nad narzędziami:
- **CMake** – konfiguracja i budowanie projektu,
- **GTest** – uruchamianie testów jednostkowych i integracyjnych,
- **System V IPC tools** (`ipcs`, `ipcrm`) – zarządzanie zasobami IPC,
- **Docker / Docker Compose** – uruchamianie środowiska izolowanego,
- **Doxygen** – generacja dokumentacji technicznej.

### 8.1 Zmienne i konfiguracja środowiska

Na początku pliku zdefiniowano kluczowe zmienne konfiguracyjne, takie jak:
- katalogi robocze (`build`, `logs`, `docs`, `docker`),
- kompilatory (`clang`, `clang++`),
- lista plików źródłowych wykorzystywana przez `clang-format`,
- nazwa paczki dystrybucyjnej.

Dodatkowo zdefiniowano kody kolorów ANSI, które poprawiają czytelność komunikatów
podczas wykonywania poleceń.

### 8.2 Budowanie projektu

- `build`:
- tworzy katalogi robocze,
- uruchamia konfigurację CMake,
- kompiluje wszystkie binaria projektu.

Proces ten jest deterministyczny i możliwy do wielokrotnego powtórzenia,
co jest kluczowe w kontekście projektów systemowych.

- `rebuild` realizuje pełne czyszczenie środowiska i ponowną kompilację.

### 8.3 Uruchamianie symulacji

`run` uruchamia kompletną symulację systemu, w tym wszystkie procesy robocze,
korzystając ze skryptu `run.sh`.

- `terminal` umożliwia dołączenie do interaktywnej konsoli operatora
pod warunkiem, że zasoby IPC zostały już zainicjalizowane przez proces Master.

### 8.4 Zarządzanie zasobami IPC

Makefile zawiera dedykowane cele do inspekcji i czyszczenia zasobów IPC:

- `make ipc` – wyświetla aktywne segmenty SHM, SEM i MSG użytkownika,
- `make ipc-clean` – usuwa zasoby IPC należące do bieżącego użytkownika,
- `make ipc-fclean` – awaryjne czyszczenie wszystkich zasobów IPC w systemie
  (wymaga uprawnień administratora).

Mechanizmy te zapobiegają pozostawianiu osieroconych zasobów IPC po awarii programu.

### 8.5 Testowanie i jakość kodu

`test` uruchamia pełną suitę testów jednostkowych i integracyjnych
z wykorzystaniem CTest.

Dodatkowo:
- `format` automatycznie formatuje kod źródłowy przy użyciu `clang-format`,
- `lint` weryfikuje zgodność stylu
