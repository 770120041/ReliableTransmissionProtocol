# RelibaleTransmissionProtocol
## About
This project used UDP to implement a transport protocol with properties similar to TCP, including the feature of congestion control. 

This project is also TCP friendliness, a TCP instance and an instance of my protocol can share a link roughly evenly
## Environments
I used different VMs to test my program, the MTU of the network is 1500, and the network connection between sender(can be multiple senders) and receivers can be
good (and the same goes for testing on the localhost), so the test environment has to be restrained by using the tc
command (you only need to run this command in the Sender VM). An example is given as follows:

Assuming the Sender VM’s network interface that connects to the Receiver VM is eth0, run the following
commands in the Sender VM:
``` 
sudo tc qdisc del dev eth0 root 2>/dev/null
sudo tc qdisc add dev eth0 root handle 1:0 netem delay 20ms loss 5%
sudo tc qdisc add dev eth0 parent 1:1 handle 10: tbf rate 100Mbit burst
40mb latency 25ms
```

The first command will delete any existing tc ruls.

The second and the third commands will give you a 100 Mbit, ~20 ms RTT link where every packet sent has a 5%
chance to get dropped (remove “loss 5%” part if you want to test your programs without artificial drops.) Note
that it’s also possible to specify the chance of reordering by adding “reorder 25%” after the packet loss rate
argument.

## Usages
```./reliable_sender <rcv_hostname> <rcv_port> <file_path_to_read> <bytes_to_send>
./reliable_receiver <rcv_port> <file_path_to_write>
```
* The `<rcv_hostname>` argument is the IP address where the file should be transferred.
* The `<rcv_port>` argument is the UDP port that the receiver listens.
* The `<file_path_to_read>` argument is the path for the binary file to be transferred.
* The `<bytes_to_send>` argument indicates the number of bytes from the specified file to be sent to the receiver.
* The `<file_path_to_write>` argument is the file path to store the received data.

Examples:
```
First, on the Receiver VM, run:
./reliable_receiver 2019 /path/to/rcv_file
Then, on the Sender VM, run:
./reliable_sender 192.168.4.38 2019 /path/to/readonly_file.dat 1000
```

## Implementation Details
I implemented a sender and a receiver, they can be deployed in different or same machine, as long as they can establish an UDP socket. And my implementation can reliably transfer 65MB files in 20S in a 40Mbps link (Usage is about half of the theoretical maximum link speed).



#### Receiver’s Function:

```void reliablyReceive(unsigned short int myUDPport, char* destinationFile)```

This function is reliablyTransfer’s counterpart and should write whatever it receives to a file called destinationFile. 

The implementation of Receiver is fairly simple, receiver will firstly listen to a specific UDP socket, then it will handle the TCP handshake when there is an incoming transmission. And the receiver can decide total packet number from MTU(which is fixed as 1500 in my program) and total length of the file to transfer. The tricky thing is that the handshake packet can also be lost and resent so the receiver need to handle the tcp packet retransmission problem.

After the three-way handshake, the receiver will store all packet into hashtable and after receiving a packet, send accumulative acks.

#### Sender’s Function:

```
void reliablyTransfer(char* hostname, unsigned short int hostUDPport,
char* filename, unsigned long long int bytesToTransfer)
```

This function should transfer the first bytesToTransfer bytes of filename to the receiver at

hostname:hostUDPport correctly and efficiently, even if the network drops and reorders some of your packets.

The implementation of Sender's function is kind of complicated, the sender needs to control the congestion control process, so I use the `State` variable to monitor the state of the sender. During the transmission, the sender will monitor the numebr of lost packets and duplicate acks to determine the next state to accordingly change the sender windows size.


## Miscellaneous
This is a course project for Computer Networks in Univeristy of Illinois at Urbana-Champaign.