#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#if 1
#define test_puts        puts
#define test_printf      printf
#else
#define test_puts(...)
#define test_printf(...)
#endif

/*
 * Multibyte values are transmitted big-endian in SiRF. SiRF single and double
 * are IEEE 754 binary32/binary64 formatted. It is assumed that the platform
 * also uses this format, so no additional processing is applied.
 */

static void
be_to_native(uint8_t *in, uint8_t *out, size_t size)
{
    // Compiler will optimize out the endian check
    int n = 1;
    if (*(char *)&n == 1) {
        for (size_t i = 0; i < size; i++) {
            out[size - i - 1] = in[i];
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
    }
}

struct gps_data {
    int32_t     latitude, longitude;
    uint32_t    satellites;
    uint16_t    nav_valid, nav_type, second;
    uint16_t    hdop, vdop, pdop;
    int32_t     altitude_ellip;
    uint32_t    ehpe, evpe, ehve;
    uint8_t     year, month, day, hour, minute;
};

static struct gps_data data;

struct field {
    uint8_t     struct_offset,
                payload_offset,
                size;
};

#define DEF_FIELD(name, size, offset) \
    { (uint8_t)offsetof(struct gps_data, name), offset, size },

//    field name        field size      offset in payload
#define MID41_FIELDS(X)                     \
    X(nav_valid,        2,              0)  \
    X(nav_type,         2,              2)  \
    X(year,             2,              8)  \
    X(month,            1,              10) \
    X(day,              1,              11) \
    X(hour,             1,              12) \
    X(minute,           1,              13) \
    X(second,           2,              14) \
    X(satellites,       4,              16) \
    X(latitude,         4,              20) \
    X(longitude,        4,              24) \
    X(altitude_ellip,   4,              28) \
    X(ehpe,             4,              43) \
    X(evpe,             4,              47) \
    X(ehve,             4,              55)

static const struct field mid41_fields[] = {
    MID41_FIELDS(DEF_FIELD)
};

#define MID66_FIELDS(X)                     \
    X(pdop,             2,              6)  \
    X(hdop,             2,              8)  \
    X(vdop,             2,             10)

static const struct field mid66_fields[] = {
    MID66_FIELDS(DEF_FIELD)
};

static void
update_gps_data(
    struct gps_data *data,
    uint8_t *payload,
    const struct field *fields,
    size_t nfields)
{
    for (unsigned i = 0; i < nfields; i++) {
        const struct field *f = &(fields[i]);
        uint8_t *b = payload + f->payload_offset;
        uint8_t *dest = (uint8_t *)data + f->struct_offset;
        be_to_native(b, dest, f->size);
    }
}

static void handle_mid6(uint8_t *payload)
{
    // Version strings are 81 bytes max including NUL
    static const unsigned N = 81;

    char *sirf = (char *)payload;
    char *nul = memchr(sirf, '\0', N);
    if (nul == NULL) {
        // Message appears to be malformed
        return;
    }

    // Check version string(s) and update if necessary
}

static void handle_message(uint8_t *msg)
{
    uint8_t mid = msg[0];
    uint8_t *payload = &(msg[1]);
    switch (mid) {
    case 41:
        update_gps_data(&data, payload, mid41_fields, ARRAY_LEN(mid41_fields));
        test_printf("(%f, %f)\n",
            (double)data.latitude * 1e-7, (double)data.longitude * 1e-7);
        break;
    case 66:
        update_gps_data(&data, payload, mid66_fields, ARRAY_LEN(mid66_fields));
        test_printf("pdop=%u,hdop=%u,vdop=%u\n", data.pdop, data.hdop, data.vdop);
        break;
    case 6:
        handle_mid6(payload);
        break;
    }
}

static size_t scan_bytes(uint8_t *buf, size_t n)
{
    if (n < 10) {
        return 0;
    }

    unsigned d;
    for (d = 0; d < (n - 10); d++) {
        if (buf[d] == 0xA0 && buf[1] == 0xA2) {
            break;
        }
    }

    if (buf[d] != 0xA0 || buf[1] != 0xA2) {
        return d;
    }

    uint8_t *frame = &(buf[d]);
    unsigned len = ((unsigned)frame[2] << 8) | frame[3];
    unsigned last = d + len + 7;
    if (len >= 2048) {
        // Not a valid frame header
        return d + 2;
    } else if (last >= n) {
        // Don't have the full frame yet
        return d;
    }

    if (frame[last - 1] != 0xB0 || frame[last] != 0xB3) {
        return d + 2;
    }

    unsigned sum = 0;
    for (unsigned i = 4; i <= (last - 4); i++) {
        sum += frame[i];
    }
    unsigned recv_xsum = ((unsigned)frame[last - 3] << 8) | frame[last - 2];
    if ((sum & 0x7FFF) != recv_xsum) {
        return d + 2;
    }

    handle_message( &(frame[4]) );

    return last + 1;
}

#include <unistd.h>

static void test_from_stdin(void)
{
    const unsigned N = 2000;
    uint8_t buf[N];
    size_t n = 0;
    for (;;) {
        n += read(STDIN_FILENO, &(buf[n]), N - n);
        test_printf("\nread %lu bytes\n", n);
        if (n < 10) {
            break;
        }

        // Process all the fully received messages
        size_t d = 0, dd;
        do {
            dd = scan_bytes( &(buf[d]), n - d );
            d += dd;
        } while (dd > 0);
        test_printf("processed %lu bytes\n", d);
        // (log and) discard processed bytes
        memmove(buf, &(buf[d]), n - d);
        n = n - d;
    }

    test_printf("%lu trailing bytes\n", n);
}

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    test_from_stdin();

    return 0;
}
