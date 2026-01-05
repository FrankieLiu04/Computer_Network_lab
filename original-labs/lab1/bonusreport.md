# Report on GBN 

This report provides an overview of the functions A_output, A_input, B_input, and A_packet_time_interrupt from the provided code, along with their respective flowcharts.

## 1. Function: A_output 

**Description:** 
The A_output function is responsible for handling outgoing packets from the sender side (Entity A). It checks if the window is full before creating and sending a new packet. If the window is full, it buffers the message for later transmission.

**Flowchart:** 

```
Start
  |
  v
Is the window full? --Yes--> Buffer the message
  |                           |
  |                           v
  No                          End
  |
  v
Create a new packet
  |
  v
Compute checksum for the packet
  |
  v
Buffer the packet in the window
  |
  v
Send the packet to layer 3
  |
  v
Start timer if base == nextseqnum - 1
  |
  v
Increment nextseqnum
  |
  v
End
```

## 2. Function: A_input 

**Description:** 
The A_input function processes incoming packets at Entity A. It checks if the packet is corrupted and handles acknowledgments (ACKs) accordingly. If the ACK is valid, it updates the base of the window and stops the timer.

**Flowchart:** 

```
Start
  |
  v
Is the packet corrupted? --Yes--> Print corruption message --> End
  |
  No
  |
  v
Get the ACK number
  |
  v
Is the ACK number valid? --No--> Print invalid ACK message --> End
  |
  Yes
  |
  v
Is the ACK number within the expected range? --No--> Print old ACK message --> End
  |
  Yes
  |
  v
Update the base of the window
  |
  v
Stop the timer for the current base
  |
  v
Resend any buffered messages if the window is not full
  |
  v
End
```

## 3. Function: B_input 

**Description:** 
The B_input function handles incoming packets at the receiver side (Entity B). It checks if the packet is corrupted or out of order. If the packet is valid, it sends the data to layer 5 and generates an acknowledgment (ACK).

**Flowchart:** 

```
Start
  |
  v
Is the packet corrupted? --Yes--> Print corruption message --> End
  |
  No
  |
  v
Is the packet in order? --No--> Print out-of-order message --> Send cumulative ACK --> End
  |
  Yes
  |
  v
Send the packet data to layer 5
  |
  v
Generate and send an ACK for the packet
  |
  v
End
```

## 4. Function: A_packet_time_interrupt 

**Description:** 
The A_packet_time_interrupt function is triggered when a packet's timer expires. It resends all packets in the current window and restarts the timer.

**Flowchart:** 

```
Start
  |
  v
Does the expired timer match the current base? --No--> End
  |
  Yes
  |
  v
Resend all packets in the current window
  |
  v
Restart the timer for the current base
  |
  v
Increment the timeout counter
  |
  v
End
```

## 5. Output sample 

```
frankieliu@FrankiedeMacBook-Air bonus % ./gbn
*************************************************************************************
**********************
         Lab1: Reliable Data Transfer Protocol-SR          
*************************************************************************************
**********************
Enter the number of messages to simulate: 
6
Enter time_ between messages from sender's layer5 [ > 0.0]:
2
Enter channel pattern string
xoo--ooooooooooooooooooooooooo
Enter sender's window size
4
*************************************************************************************
**********************
                                        Packet Transmission Log                                         
*************************************************************************************
**********************
[2.0] A: send packet [0] base [0]
[4.0] A: send packet [1] base [0]
[4.0] A: start timer for packet [0]
[6.0] A: send packet [2] base [0]
[8.0] A: send packet [3] base [0]
[10.0] A: buffer packet [4] base [0]
[10.5] B: packet [1] out of order, expected [0]
[12.0] A: buffer packet [4] base [0]
[12.5] B: packet [2] out of order, expected [0]
[14.5] B: packet [999999] corrupted
[17.0] A: ACK corrupted
[19.0] A: packet [0] time_ out, resend window
[19.0] A: resend packet [0]
[19.0] A: resend packet [1]
[19.0] A: resend packet [2]
[19.0] A: resend packet [3]
[19.0] A: received old ACK [-1], current base [0]
[25.5] B: packet [0] received, send ACK [0]
[25.5] B: packet [1] received, send ACK [1]
[25.5] B: packet [2] received, send ACK [2]
[25.5] B: packet [3] received, send ACK [3]
[32.0] A: ACK corrupted
[32.0] A: received ACK [1]
[32.0] A: stop timer for packet [0]
[32.0] A: start timer for packet [2]
[32.0] A: send packet [4] base [2]
[32.0] A: send packet [5] base [2]
[32.0] A: received ACK [2]
[32.0] A: stop timer for packet [2]
[32.0] A: start timer for packet [3]
[32.0] A: received ACK [3]
[32.0] A: stop timer for packet [3]
[32.0] A: start timer for packet [4]
[38.5] B: packet [4] received, send ACK [4]
[38.5] B: packet [5] received, send ACK [5]
[45.0] A: received ACK [4]
[45.0] A: stop timer for packet [4]
[45.0] A: start timer for packet [5]
[45.0] A: received ACK [5]
[45.0] A: stop timer for packet [5]
Simulator terminated.
*************************************************************************************
**********************
                                       Packet Transmission Summary                                          
*************************************************************************************
**********************
From Sender to Receiver:
 total sent pkts:         10
 total correct pkts:      6
 total resent pkts:       4
 total lost pkts:         0
 total corrupted pkts:    1
 the overall throughput is: 34.133 Kb/s
```

## Summary 

- **A_output:** Handles outgoing packets, buffering them if necessary, and starts timers.
- **A_input:** Processes incoming ACKs, updates the window, and manages timers.
- **B_input:** Validates incoming packets, sends data to layer 5, and generates ACKs.
- **A_packet_time_interrupt:** Resends packets and restarts timers upon timeout.

These flowcharts provide a clear understanding of how each function operates within the protocol implementation.

Based on the output, functionally the code has implemented the basic logic of the gbn transfer, but in my testing it would often appear that the code would get stuck in a dead loop on certain inputs. Due to time constraints and capacity limitations, I am unable to improve the problem further.
