import socket
import sys
import time

if len(sys.argv) != 4:
    print('%d,arguments', len(sys.argv), sys.argv[0], sys.argv[1], int(sys.argv[2]), sys.argv[3])
    print(
        'Usage: python3 udp_packet_send.py [dest IPv6] [dest port] [message]' )
    exit(1)

sock = socket.socket(socket.AddressFamily.AF_INET6, socket.SocketKind.SOCK_DGRAM)
#sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 10)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, bytes("tun0", 'ascii'))
sock.sendto(bytearray(sys.argv[3], "utf-8"), (sys.argv[1], int(sys.argv[2])))

print('packet sent')
