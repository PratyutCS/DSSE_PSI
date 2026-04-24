# Lambda Optimized DSSE: Bit-Level Technical Analysis

This document provides a strictly verified walkthrough of the Lambda Optimized DSSE system's performance across update, search, and deletion operations. The following example demonstrates correctly handling a range query [48, 51] using the system's existing markers to resolve deletions in O(1) time.

---

## 1. Initial State: Update (IDs 0–5)
We initialize the system with 6 identifiers (IDs 0–5) corresponding to Keywords 48–53. The keyword space is split at the midpoint **50**.

### Marker Generation Map
Each ID registers itself into a specific set of range markers (Prefix 0) and a point marker (Prefix 1).

| ID | Keyword | Markers Updated |
| :--- | :---: | :--- |
| **0** | 48 | 049, 050, 148 |
| **1** | 49 | 050, 149 |
| **2** | 50 | 150 |
| **3** | 51 | 051, 151 |
| **4** | 52 | 051, 052, 152 |
| **5** | 53 | 051, 052, 053, 153 |

---

## 2. Phase 1: Search Trace [48, 51] (Pre-Deletion)
Goal: Find IDs with Keywords between 48 and 51 inclusive.

### Search Execution
1.  **Lower Boundary (48–50):**
    - **Step A:** Fetch Membership set $S_0 = search(050) \cup search(150) = \{0, 1\} \cup \{2\} = \{0, 1, 2\}$.
    - **Step B:** $GE_{48} = S_0 \cap \text{NOT } search(048)$ = **{0, 1, 2}**.
2.  **Upper Boundary (51):**
    - **Step A:** Fetch Membership set $S_1 = search(051) = \{3, 4, 5\}$.
    - **Step B:** $LE_{51} = S_1 \cap \text{NOT } search(052)$ = $\{3, 4, 5\} \setminus \{4, 5\}$ = **{3}**.

**Pre-Deletion Result:** {0, 1, 2} $\cup$ {3} = **{ID 0, ID 1, ID 2, ID 3}**. (Correct).

---

## 3. Phase 2: Deletion Trace (ID 1 / Keyword 49)
We delete **ID 1 (Keyword 49)** by sending an update for Keyword 49 with an "op" bit of **0**.

### Resulting System State
Marker **050** (the membership list for $K \le 49$) receives the delete record. Due to the server's LIFO (Last-In, First-Out) resolution, subsequent searches for this marker will result in:

| Marker | Previous Result | New Result (after op=0) |
| :--- | :---: | :---: |
| **050** | {0, 1} | **{0}** |
| **149** | {1} | **{}** |

---

## 4. Phase 3: Search Trace [48, 51] (Post-Deletion)
We perform the same range query after ID 1 is deleted.

### Re-Calculated Result
1.  **Lower Boundary:**
    - **New S0:** $search(050) \cup search(150) = \{0\} \cup \{2\} = \{0, 2\}$.
    - **GE 48:** $S_0 \cap \text{NOT } search(048)$ = **{0, 2}**.
2.  **Upper Boundary:**
    - **LE 51:** Still results in **{3}**.

**Final Result Table**
| Identifier | ID 0 (48) | ID 1 (49) | ID 2 (50) | ID 3 (51) |
| :--- | :---: | :---: | :---: | :---: |
| **Initial Bit (Add)** | 1 | 1 | 1 | 1 |
| **New Status Mask ($S_0/S_1$)** | 1 | **0** | 1 | 1 |
| **POST-DELETION RESULT** | **1** | **0** | **1** | **1** |

**Final Found IDs:** {0, 2, 3}.
Keyword 49 (ID 1) has successfully disappeared from the results.

---

## 5. Efficiency Audit
- **Search Time:** O(1). Exactly 4 boundary-lookup operations are performed.
- **Storage:** No additional global existence list or extra server-side space is required.
- **Correctness:** The sector-level status bits ($S_0, S_1$) provide a definitive "Alive" filter that prevents phantom bits from re-appearing during logical negations.
