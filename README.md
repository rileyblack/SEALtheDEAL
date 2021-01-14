# SEALtheDEAL
Securitized banking model designed from scratching using Linux and C which protects against multi-user threading fraud by employing the [mutex](https://en.cppreference.com/w/cpp/thread/mutex#:~:text=The%20mutex%20class%20is%20a,try_lock%20until%20it%20calls%20unlock%20.) synchronization primitive to prevent simultaneous shared data access.

## Structure
Security and functionality are confirmed by simultaneously processing multiple transactional threads which are constructed by parsing user-designed files describing scenarios
of varying size/complexity. Results are published to a summary report.

Supports any number of the following entities:
* Accounts
* Depositors
* Clients

Supports any number of the following actions:
* Deposits
* Transfers
* Withdrawls
* Overdrafts (Fees/restrictions applicable based on account)

## Practical Take-Away Experience
* C
* Threading synchronization
* Linux command-line environments
* Data Structures and Algorithms
* File Parsing
* Dynamic Allocation
* Pointers
