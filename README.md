# Magazyn Firmy Spedycyjnej – Raport Projektowy

**Autor:** [Robert Moń]
**Nr albumu:** [MINISTRY-OF-TRUTH-HAS-CENSORED-THIS-LINE]
**Repozytorium:** [https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej]

---

## 1. Założenia i Architektura Systemu

Symulacja funckjonuje jako wieloprocesowa,
oparta na architekturze Master–Slave/Master-Worker.
Proces nadrzędny odpowiada za inicjalizację zasobów IPC
oraz zarządzanie w maksymalnie prosty sposób cyklem życia procesów potomnych.

### Kluczowe procesy

- **Master (main)**
  Tworzy segmenty IPC, wykonuje `fork()` i `execv()` dla workerów,
  obsługuje sygnały oraz sprząta zasoby systemowe.

- **Belt**
  Producent paczek. Zarządza buforem cyklicznym w pamięci współdzielonej.

- **Dispatcher**
  Konsument. Pobiera paczki z taśmy i synchronizuje załadunek do ciężarówek.

- **Truck**
  Symulator logistyki – reaguje na sygnały z kolejki komunikatów.

- **Express (VIP)**
  Proces wysokiego priorytetu, generujący paczki omijające standardową kolejkę.

- **Terminal**
  Interaktywna konsola operatora (HMI) z kontrolą dostępu **RBAC**.

---

## 2. Wykorzystane Konstrukcje Systemowe

### A. Tworzenie i zarządzanie procesami

Zastosowano funkcje `fork()` i `execv()`,

**Funkcje:**
- `fork()`
- `execv()`
- `waitpid()`
- `kill()`

**Kod:**
[src/main.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp) – funkcja `spawnChild`

---

### B. Synchronizacja – Semafory

Semafory służą do:
- implementacji problemu producent–konsument,
- ochrony sekcji krytycznych,
- synchronizacji dostępu do bufora cyklicznego.
- ale przede wszystkim ochrony atomowości operacji na pamięci współdzielonej

**Funkcje:**
- `semget()`
- `semctl()`
- `semop()`

**Kod:**
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp) – metoda `semOperation`

---

### C. Komunikacja – Pamięć Współdzielona

Segment SHM przechowuje:
- stan taśmy produkcyjnej,
- dane doku załadunkowego,
- tabelę aktywnych sesji użytkowników.

**Funkcje:**
- `shmget()`
- `shmat()`
- `shmdt()`
- `shmctl()`

**Uprawnienia:** `0600` (tylko właściciel)

**Kod:**
[include/Shared.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Shared.h) – struktura `SharedState`
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h) – konstruktor

---

### D. Komunikacja – Kolejki Komunikatów

Kolejki służą do asynchronicznego przesyłania sygnałów sterujących:
- wymuszenie odjazdu ciężarówki,
- zatrzymanie awaryjne systemu.

**Funkcje:**
- `msgget()`
- `msgsnd()`
- `msgrcv()`
- `msgctl()`

**Kod:**
[include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h) – metoda `sendSignal`

---

### E. Obsługa sygnałów

System obsługuje:
- `SIGINT` – bezpieczne zakończenie i sprzątanie zasobów,
- `SIGTERM` – zamykanie procesów potomnych.

**Kod:**
[src/main.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp) – funkcja `handleSigint`

---

## 3. Testy

Projekt posiada "duże" pokrycie testów jednostkowych, co zminimalizowało
ryzyko deadlocków i błędów synchronizacji.

### A. Testy jednostkowe i integracyjne

| Moduł | Zakres testów | Plik testowy |
| :--- | :--- | :--- |
| **Belt** | Bufor cykliczny, FIFO, wrap-around | [belt_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/belt_test.cpp) |
| **Config** | Zmienne środowiskowe, fallback | [config_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/config_test.cpp) |
| **Dispatcher** | TOCTOU race condition, retry-loop | [dispatcher_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/dispatcher_test.cpp) |
| **Express** | Priorytetyzacja VIP, bezpośredni załadunek | [express_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/express_test.cpp) |
| **Manager** | Pełny cykl życia zasobów IPC (SHM/SEM/MSG) | [manager_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/manager_test.cpp) |
| **Session** | Autoryzacja RBAC, limity procesów (quota) | [session_manager_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/session_manager_test.cpp) |
| **Shared** | Integralność struktur w pamięci dzielonej | [shared_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/shared_test.cpp) |
| **Terminal** | Stream hijacking, symulacja wejścia użytkownika | [terminal_manager_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/terminal_manager_test.cpp) |
| **Truck** | Maszyna stanów (FSM) logistyki pojazdów | [truck_test.cpp](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/test/truck_test.cpp) |
---
## 5. Dokumentacja Techniczna

- Procesy (fork/exec): [src/main.cpp – spawnChild](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp)
- Sygnały: [src/main.cpp – handleSigint](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/src/main.cpp)
- Semafory: [include/Manager.h – semOperation](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h)
- SHM: [include/Manager.h – constructor](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h)
- Kolejki: [include/Manager.h – sendSignal](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h)
- Błędy (`errno`): [include/Manager.h](https://github.com/Moniev/SO-magazyn-firmy-spedycyjnej/blob/main/include/Manager.h)

---

## 6. Instrukcja Uruchomienia i Zarządzania

### A. Budowanie i uruchamianie

- `make build` – konfiguracja CMake i kompilacja binarek
- `make run` – uruchomienie Mastera
- `make terminal` – uruchomienie konsoli operatora

---

### B. Testowanie i jakość kodu

- `make test` – pełna suita testów
- `make format` – formatowanie kodu (`clang-format` musi być zainstalowany, to zależność która jest w pakiecie clang w większości repozytoriów)
- `make lint` – statyczna analiza stylu

---

### C. Zarządzanie zasobami IPC

- `make ipc` – lista aktywnych zasobów IPC
- `make ipc-clean` – usunięcie zasobów użytkownika
- `make ipc-fclean` – wymuszone czyszczenie

---

### D. Docker
- `make docker-build` – budowa obrazu
- `make docker-run` – uruchomienie symulacji
- `make docker-terminal` – terminal w kontenerze

---

### E. Dokumentacja i dystrybucja

- `make docs` – generacja dokumentacji Doxygen
- `make package` – paczka dystrybucyjna `.tar.gz`

---

## 7. Podsumowanie i Wnioski

Projekt umożliwił praktyczne poznanie synchronizacji procesów, IPC i obsługi sygnałów.
Największym problemem była eliminacja race condition z maina terminal managera i zapewnienie
atomowości operacji na pamięci współdzielonej.
