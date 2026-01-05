# Report on SR 

## 1. Compilation Environment 

The code was compiled and executed in the following environment:

- **Operating System:** macOS (Latest Version)
- **IDE/Editor:** Visual Studio Code (VS Code)
- **Compiler:** GCC (GNU Compiler Collection), installed via Homebrew
- **Compilation Command:** `gcc -o stable stable.c`
- **Execution Command:** `./stable`

## 2. Flowcharts of Key Functions 

### 2.1 A_output Function Flowchart 

```
Start
  |
  v
Check if nextseqnum < base + WINDOWSIZE
  |-----------------------------|
 Yes                           No
  |                             |
  v                             v
Create new packet             Check if buffer is full
  |                             |
  v                             v
Send packet to layer3         Buffer message if space available
  |                             |
  v                             v
Save packet to winbuf         Exit if buffer is full
  |
  v
Set timer for packet
  |
  v
Increment nextseqnum
  |
  v
End
```

### 2.2 A_input Function Flowchart 

```
Start
  |
  v
Check if packet is corrupted
  |-----------------------------|
 Yes                           No
  |                             |
  v                             v
Discard packet                Check if ACK is valid
                                |
                                v
                            Stop timer for ACKed packet
                                |
                                v
                            Slide window forward if possible
                                |
                                v
                            Send new packets from buffer if window allows
                                |
                                v
                            End
```

### 2.3 B_input Function Flowchart 

```
Start
  |
  v
Check if packet is corrupted
  |-----------------------------|
 Yes                           No
  |                             |
  v                             v
Discard packet                Check if packet is within expected range
                                |
                                v
                            Buffer packet and send ACK
                                |
                                v
                            Deliver in-order packets to layer5
                                |
                                v
                            Update expected sequence number
                                |
                                v
                            End
```

### 2.4 A_packet_time_interrupt Function Flowchart 

```
Start
  |
  v
Find packet in buffer by seqnum
  |-----------------------------|
 Yes                           No
  |                             |
  v                             v
Resend packet                 Do nothing
  |
  v
Restart timer for packet
  |
  v
End
```

## 3. Issues Encountered and Solutions 

### 3.1 Issue: Total Sent Packets Count Less Than Expected 

**Problem Description:** The total number of sent packets (packet_sent) was less than the expected count.

**Root Cause:** A syntax error in the counter increment logic caused some packets to be missed during counting.

**Solution:** Reviewed and corrected the increment logic in the A_output function to ensure every sent packet is counted.

### 3.2 Issue: Throughput Calculation Error 

**Problem Description:** The throughput calculation was incorrect due to a mismatch in time units.

**Root Cause:** The time unit used in the calculation was inconsistent with the actual simulation time.

**Solution:** Standardized the time unit to seconds and recalculated throughput as `(packet_receieve * 32.0 * 8) / currenttime_()`.

### 3.3 Issue: Window Did Not Slide After All Packets in Window Were Sent

**Problem Description:** The sender window did not slide forward even after all packets in the window were acknowledged.

**Root Cause:** The logic to slide the window forward was not triggered correctly when all packets were acknowledged.

**Solution:** Added a loop in the A_input function to slide the window forward until the first unacknowledged packet is found.

### 3.4 Issue: B Did Not Receive Resent Packets After Timeout 

**Problem Description:** After a timeout, the sender resent packets, but the receiver did not process them.

**Root Cause:** The receiver's logic did not handle duplicate packets correctly, leading to missed acknowledgments.

**Solution:** Enhanced the B_input function to handle duplicate packets by checking if they are already buffered and sending an ACK if necessary.

## 4. Conclusion 

The implementation of the reliable data transfer protocol was successfully completed after addressing the above issues. The flowcharts provided a clear understanding of the logic flow, and the solutions ensured the correctness and robustness of the protocol. Future improvements could focus on optimizing buffer management and reducing redundant computations.
