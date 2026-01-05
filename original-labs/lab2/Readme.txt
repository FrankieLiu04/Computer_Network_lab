Project Overview

This project implements a robust client-server file management system that allows users to perform various file operations between a client and a server over a network. The system utilizes both UDP for command transmission and TCP for large file transfers, demonstrating a practical application of network programming concepts.

Features Implemented

Core Functionality

File Listing (ls): View all files stored on the server's backup directory.
File Upload (send): Transfer files from client to server with support for both small and large files.
File Removal (remove): Delete files from the server's backup directory.
File Renaming (rename): Change filenames of files stored on the server.
Server Shutdown (shutdown): Remotely shut down the server.
Client Exit (quit): Gracefully exit the client application.

Advanced Features

Dual-Protocol Implementation:
UDP for command transmission and small file transfers
TCP for large file transfers, automatically selected based on file size
Robust Error Handling:
File existence checks
Overwrite confirmations
Network timeout detection
Error recovery mechanisms
Large File Support:
Chunked file transfer for efficient handling of large files
Progress tracking during transmission
Dynamic TCP port allocation
User Interaction:
Interactive command-line interface
Clear feedback on operation status
Confirmation prompts for potentially destructive operations

System Performance in Single-Device Environment

When running both client and server on the same device or in a low-latency local network environment, the system demonstrates excellent performance characteristics:

Seamless Command Execution: All commands respond promptly with minimal latency.
Reliable File Operations: File transfers, renames, and deletions work consistently without errors.
Efficient Large File Handling: The TCP-based transfer mechanism efficiently handles files of various sizes.
Robust Error Recovery: The system gracefully handles edge cases such as file overwrites and non-existent files.
Protocol Switching: The transition between UDP and TCP protocols occurs transparently to the user.

The single-device testing environment masks potential timing issues due to the negligible network latency, presenting a seemingly flawless system.

Cross-Device Challenges

When deployed across physically separated devices with real-world network conditions, several issues emerge that highlight the importance of proper synchronization in distributed systems:

Key Issues Identified:

Rename Operation Timing Problem:
In the rename operation, the client sends the new filename too early, before the server is ready to receive it.
This race condition is masked in local testing due to minimal latency but becomes apparent in real network conditions.
The server needs to explicitly acknowledge its readiness to receive the new filename before the client sends it.
Command Synchronization:
Network latency exposes the lack of proper handshaking between state transitions.
Some commands need intermediate acknowledgment steps to ensure proper sequence of operations.
Timeout Handling:
Remote connections may experience longer delays, requiring more sophisticated timeout and retry mechanisms.
Data Loss Prevention:
The system needs more robust mechanisms to ensure data integrity when faced with network instability.

Lessons Learned

This project demonstrates the critical importance of:

Proper Protocol Design: Network protocols must account for real-world latency and not assume ideal conditions.
State Synchronization: Explicit acknowledgment between state transitions is essential for distributed systems.
Testing in Realistic Environments: Testing across different network conditions reveals issues hidden in local testing.
Defensive Programming: Always assume network communications may fail and handle these cases gracefully.

The implementation showcases a functional file management system while also highlighting the challenges inherent in distributed application development.
