# rvs: a virtual switch, aiming to be rapid and platform-independent

rvs is a rapid virtual switch that aims to be platform-independent so that it can be integrated into a wide range of systems.

rvs forwards packets from a virtual interface, called rvif, to other rvif(s); each rvif is instantiated over a shared memory file, and can be associated with a process/system through the file interface.

The forwarding plane of rvs is inspired by [VALE](https://dl.acm.org/doi/10.1145/2413176.2413185)/[mSwitch](https://dl.acm.org/doi/10.1145/2774993.2775065) that is a [netmap](https://www.usenix.org/conference/atc12/technical-sessions/presentation/rizzo)-based virtual switch.

The primary motivation of rvs is to have the simplest implementation of a high-performance virtual switch, which does not depend on external libraries; netmap is highly flexible and sophisticated, however, it is sometimes too much for small tests and requires a bit of engineering to port specific components (e.g., the packet switching logic of VALE) to other environments (e.g., running the VALE switch in user-space, rather than in the kernel).

## How to use

This repository contains two example applications: an rvs packet forwarder (apps/fwd) and a packet generator (apps/pkt-gen).

### Compilation

Please enter the top directory of this repository.

```
cd rvs
```

The following compiles the fwd application.

```
make -C apps/fwd
```

The following compiles the pkt-gen application.

```
make -C apps/pkt-gen
```

### Example

Let's forward packets between two pkt-gen processes through the fwd application; we try the topology illustrated below.

```
[pkt-gen app tx] --> [fwd app] --> [pkt-gen app rx]
```

First, please create two of 32 MB shared memory files by the following commands; in this example, ```/dev/shm/rvs_shm00``` is used as the rvif for the sender, and ```/dev/shm/rvs_shm01``` is used for the rvif of the receiver.

```
dd if=/dev/zero of=/dev/shm/rvs_shm00 bs=1M count=0 seek=32
```

```
dd if=/dev/zero of=/dev/shm/rvs_shm01 bs=1M count=0 seek=32
```

Please open a terminal/console and type the following command to run the fwd application; the following executes the rvs logic and forwards packets between rvifs made on ```/dev/shm/rvs_shm00``` and ```/dev/shm/rvs_shm01```.

```
./apps/fwd/a.out -m /dev/shm/rvs_shm00 -m /dev/shm/rvs_shm01
```

Then, please open another terminal/console and type the following command to launch the receiver pkt-gen process.

```
./apps/pkt-gen/a.out -m /dev/shm/rvs_shm01 -S 01:23:35:67:89:ab -D ff:ff:ff:ff:ff:ff -s 192.168.123.3 -d 255.255.255.255 -f rx
```

At last, please open another terminal/console and type the following command to launch the sender pkt-gen process.

```
./apps/pkt-gen/a.out -m /dev/shm/rvs_shm00 -S 01:23:35:67:89:aa -D 01:23:35:67:89:ab -s 192.168.123.2 -d 192.168.123.3 -f tx -l 64
```

The following is the example output.

fwd

```
$ ./apps/fwd/a.out -m /dev/shm/rvs_shm00 -m /dev/shm/rvs_shm01
port[0]: /dev/shm/rvs_shm00 (0x7fe5749a4000)
port[1]: /dev/shm/rvs_shm01 (0x7fe5729a4000)
-- FWD --
   0.000001 Mpps
   0.955904 Mpps
  13.261311 Mpps
  13.245440 Mpps
  13.252096 Mpps
  13.236734 Mpps
```

pkt-gen rx

```
$ ./apps/pkt-gen/a.out -m /dev/shm/rvs_shm01 -S 01:23:35:67:89:ab -D ff:ff:ff:ff:ff:ff -s 192.168.123.3 -d 255.255.255.255 -f rx
/dev/shm/rvs_shm01 is mapped at 0x7f85e455e000 (33554432 bytes)
rvif uses 10543104 bytes
src 01:23:35:67:89:ab (192.168.123.3) dst ff:ff:ff:ff:ff:ff (255.255.255.255)
-- RX --
    0.000 Mpps (    0.000 Gbps )
    8.936 Mpps (    4.575 Gbps )
   13.275 Mpps (    6.796 Gbps )
   13.262 Mpps (    6.790 Gbps )
   13.261 Mpps (    6.790 Gbps )
```

pkt-gen tx

```
$ ./apps/pkt-gen/a.out -m /dev/shm/rvs_shm00 -S 01:23:35:67:89:aa -D 01:23:35:67:89:ab -s 192.168.123.2 -d 192.168.123.3 -f tx -l 64
/dev/shm/rvs_shm00 is mapped at 0x7f9fe5902000 (33554432 bytes)
rvif uses 10543104 bytes
src 01:23:35:67:89:aa (192.168.123.2) dst 01:23:35:67:89:ab (192.168.123.3)
-- TX --
   13.245 Mpps (    6.781 Gbps )
   13.243 Mpps (    6.780 Gbps )
   13.252 Mpps (    6.785 Gbps )
   13.243 Mpps (    6.780 Gbps )
```

Here, we observe 13 Mpps (million packets per second) for 64-byte packets.

### Command options of the example applications

apps/fwd

- ```-m```: specifies a shared memory file of an rvif attached to an rvs instance
- ```-b```: batch size to forward packtes

apps/pkt-gen

- ```-d```: destination IP address set in the TX packets
- ```-D```: destination MAC address set in the TX packets
- ```-f```: specifies the role either ```rx``` or ```tx```
- ```-l```: size of the TX packets (in byte)
- ```-m```: specifies a shared memory file used as an rvif
- ```-s```: source IP address set in the TX packets
- ```-S```: source MAC address set in the TX packets
- ```-t```: number of threads

NOTE: the rx mode of apps/pkt-gen transmits a single packet during the initialization phase so that the learning bridge logic in rvs can learn the pair of the port and the rvif MAC address.

## Internals

### Packet forwarding logic

The packet forwarding logic presented in Section 3.3.1 of [the VALE paper](https://conferences.sigcomm.org/co-next/2012/eproceedings/conext/p61.pdf) is equivalent to the following parts.

Marked by ```// stage 2, compute destinations``` in the figure of the paper.
https://github.com/yasukata/rvs/blob/6ec2d454ca0e0b405503897c1e711bd9dbc64dd9/rvs.c#L40-L65

Marked by ```// stage 3, forward, looping on ports``` in the figure of the paper.
https://github.com/yasukata/rvs/blob/6ec2d454ca0e0b405503897c1e711bd9dbc64dd9/rvs.c#L67-L98

The paper has the code block ```// stage 1, build batches and prefetch```, but this part is a bit different in rvs; in rvs, the batch size is controlled through the ```batch``` argument passed to ```unsigned short rvs_fwd(struct rvs *vs, unsigned short vid, unsigned short qid, unsigned short batch)```.

To customize the packet forwarding logic, we should modify the following code block, which looks up the destination of a packet.
https://github.com/yasukata/rvs/blob/6ec2d454ca0e0b405503897c1e711bd9dbc64dd9/rvs.c#L46-L61

### rvif interface

The definition of ```struct rvif``` is as follows.
https://github.com/yasukata/rvs/blob/6ec2d454ca0e0b405503897c1e711bd9dbc64dd9/include/rvif.h#L22-L42

rvs assumes the top of ```struct rvif``` is located at the top of a shared memory file.

This rvif representation aims to allow a process/system to configure the queues of its rvif; the process/system can configure the number of queues and the number of slots/descriptors for an rvif.

```rvif.num``` represents the number of queues.

Each queue has two rings, and ring[0] is for RX and ring[1] is for TX.

```rvif.queue[queue_id].ring[0].num``` represents the number of RX slots/descriptors of the rvif.queue[queue_id].

```rvif.queue[queue_id].ring[1].num``` represents the number of TX slots/descriptors of the rvif.queue[queue_id].

Each ring has the head/tail indexes, which indicate the slots/descriptors to be consumed; if head and tail are the same, it means nothing to be consumed.

The process/system, that performs I/O over the rvif, updates tail, and rvs updates head.

Each slot has two fields ```off``` and ```len```.

```off``` is used for pointing a packet buffer at the address ```(unsnigned long) vif + slot[slot_id].off```.

rvs assumes the size of a packet buffer associated with a slot is always 2048 bytes, that is aligned by 2048 bytes.

```len``` is used for the TX path, and indicates the packet size.

### Portability of rvs

rvs aims to be as portable as possible.

While it is not mandatory, we specify ```-std=c89 -nostdlib -nostdinc``` for the CFLAGS in the Makefile of the fwd application to ensure the rvs implementation does not require external libraries.

Lock implementations are usually platform-dependent because they typically use atomic CPU operations; therefore, the rvs implementation assumes the lock implementation is provided by the application which employs the rvs code.
https://github.com/yasukata/rvs/blob/6ec2d454ca0e0b405503897c1e711bd9dbc64dd9/rvs.c#L21-L26
