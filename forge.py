# Generate known SiRF binary data for testing purposes

import sys, struct, math, random, os

def nul(n): return chr(0) * n

def frame(mid, body):
    payload =  chr(mid) + body
    n = len(payload)
    assert(n < (1 << 11))
    xsum = sum(ord(b) for b in payload) & 0x7FFF
    header = '\xA0\xA2' + chr(n >> 8) + chr(n & 0xFF)
    footer = chr(xsum >> 8) + chr(xsum & 0xFF) + '\xB0\xB3'
    return header + payload + footer

# Parameters are standard Python floats
def mid41(lat, lon):
    ilat = int(lat * 1e7)
    ilon = int(lon * 1e7)
    coord_bytes = struct.pack('>ii', ilat, ilon)
    msg = nul(20) + coord_bytes + nul(90 - 28)
    return frame(41, msg)

def mid66(pdop, hdop, vdop):
    data_bytes = struct.pack('>HHH', pdop, hdop, vdop)
    msg = nul(6) + data_bytes + nul(2)
    return frame(66, msg)

if __name__ == '__main__':
    out = sys.stdout
    for n in range(30):
        lat = n * math.pi / 30
        lon = -lat
        out.write( mid41(lat, lon) )
        out.write( mid66(n, n + 1, n + 5) )
        nrand = random.randint(0, 100)
        out.write(os.urandom(nrand))
    out.write( mid41(200.5, 100.5) )
    out.flush()
