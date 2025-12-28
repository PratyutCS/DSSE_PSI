# Private Set Intersection (PSI) using DSSE

This project implements a Private Set Intersection (PSI) protocol built on top of **Dynamic Searchable Symmetric Encryption (DSSE)**. It allows for privacy-preserving range queries and set operations using bit sequences.

## 1. DSSE (Dynamic Searchable Symmetric Encryption)
The core encryption layer uses the **FAST** scheme. DSSE allows a client to encrypt their data such that it remains searchable by a server without revealing the actual content or keywords to the server (unless a search token is provided).

- **Setup**: Initializes the encryption keys and the server-side database (RocksDB).
- **Update**: Encrypts and sends data to the server. This project pads data to a fixed length (15 bytes) to ensure consistency.
- **Search**: Generates tokens for specific keywords, allowing the server to return encrypted results which are then decrypted by the client.

## 2. Code Flow
The main logic is contained in `queen.cpp`, which follows this lifecycle:

### A. Initialization & Setup
- The DSSE system is initialized.
- A local database (`inp`) is converted and uploaded to the DSSE server using `Update_client` and `Update_server`.

### B. Interactive Search Loop
The program enters a loop asking the user for two search parameters (`param1` and `param2`). For each search:

1.  **DSSE Search**: Search tokens are generated and sent to the server to retrieve raw search results.
2.  **Bit Sequence Generation**:
    - **`less_than(n, k)`**: Returns a `BitSequence` where the first `k` bits are set to 1. This represents the "Less Than" property of the search result.
    - **`equal_bits(a, b, n)`**: Returns a `BitSequence` with bits in the range `[a, b]` set to 1.
3.  **Bitwise Operations**:
    - **Complement**: Using `ones_complement()`, we turn a "Less Than" sequence into a "Greater Than or Equal" (GE) sequence.
    - **OR**: Using `bitwise_or()`, we combine "Less Than" and "Equal" to form a "Less Than or Equal" (LE) sequence.
    - **AND**: Using `bitwise_and()`, we intersect the GE and LE sequences to find the final range intersection (`result_bitmap`).

### C. Final Output
The `result_bitmap` represents the indices where both conditions are met, effectively performing a private range intersection.

## 3. Storage & Utilities
- **`BitSequence` Class**: A high-performance class in `BitSequence.cpp` that stores large numbers of bits in `uint64_t` blocks. It supports efficient bitwise NOT, OR, and AND.
- **`run_psi.sh`**: A shell script to clean up databases, compile with `g++`, and run the `queen` binary.

## 4. How to Run
```bash
bash run_psi.sh
```
Follow the prompts to enter search parameters. Enter `0` when asked if you want to continue to exit the loop.
