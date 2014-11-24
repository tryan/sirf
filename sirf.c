#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

/*
 * Multibyte values are transmitted big-endian in SiRF. SiRF single and double
 * are IEEE 754 formatted, so no extra byte reordering/interpretation is needed
 * if native floats and doubles are also IEEE 754.
 */

static void
be2(uint8_t *b, uint16_t *out)
{
    *out = ((uint16_t)b[0] << 8) | b[1];
}

static void
be4(uint8_t *b, uint32_t *out)
{
    uint32_t x = 0;
    for (int i = 0; i < 4; i++) {
        x = (x << 8) | b[i];
    }
    *out = x;
}

static void
be8(uint8_t *b, uint64_t *out)
{
    uint64_t x = 0;
    for (int i = 0; i < 8; i++) {
        x = (x << 8) | b[i];
    }
    *out = x;
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
    uint8_t *msg,
    const struct field *fields,
    size_t nfields)
{
    for (unsigned i = 0; i < nfields; i++) {
        const struct field *f = &(fields[i]);
        uint8_t *b = msg + f->payload_offset;
        uint8_t *dest = (uint8_t *)data + f->struct_offset;
        switch (f->size) {
        case 1:
            *dest = *b;
            break;
        case 2:
            be2(b, (uint16_t *)dest);
            break;
        case 4:
            be4(b, (uint32_t *)dest);
            break;
        case 8:
            be8(b, (uint64_t *)dest);
            break;
        }
    }
}

static void handle_mid6(uint8_t *msg)
{
    // Version strings are 81 bytes max (including NUL)
    static const unsigned N = 81;

    char *sirf = (char *)msg;
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
    switch (mid) {
    case 41:
        update_gps_data(&data, msg + 1, mid41_fields, ARRAY_LEN(mid41_fields));
        break;
    case 66:
        update_gps_data(&data, msg + 1, mid66_fields, ARRAY_LEN(mid66_fields));
        break;
    case 6:
        handle_mid6(msg + 1);
        break;
    }
}

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint8_t mid41[91];
    for (int i = 0; i < 91; i++) { mid41[i] = (uint8_t)i; }
    mid41[0] = 41;
    handle_message(mid41);

#define SHOW(name, size, offset) printf(#name " = %08X\n", data.name);
    MID41_FIELDS(SHOW);

    return 0;
}
