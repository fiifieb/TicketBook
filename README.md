# 🎟️ TicketBook – Multithreaded Ticketing System

TicketBook is a multithreaded ticketing system written in **C** with inline **RISC‑V Assembly**, using a **concurrent hash table** for in-memory reservations and a **MySQL database** for persistent storage of ticket purchases.

---

## ✨ Features
- ✅ Fine-grained locking per seat to handle concurrent reservations
- ✅ Timeout-based seat holds to prevent indefinite locking
- ✅ Persistent storage in MySQL with transaction-safe updates
- ✅ Clear separation between in-memory fast lookups and database persistence

---

## 🏗️ High-Level Architecture

```mermaid
flowchart LR
    A[Client/UI] -->|HTTP APIs| B[Backend Service]
    B -->|Check seat status| C[(Concurrent Hash Table)]
    B -->|Persist purchase| D[(MySQL Database)]
    C --> B
    D --> B
    B -->|Confirmation & Updates| A