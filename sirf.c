#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum {
    T_1U = 1, T_2U = 2, T_4U = 4,
    T_1S = 1, T_2S = 2, T_4S = 4,
    T_SGL = 4, T_DBL = 8,
};

#define T_1D T_1U
#define T_2D T_2U
#define T_4D T_4U

struct gps_data {
    int32_t     latitude, longitude;
    uint32_t    satellites;
    uint16_t    nav_valid, nav_type, second;
    uint8_t     year, month, day, hour, minute;
    uint8_t     hdop;
    int32_t     altitude_ellip;
};

//    field name        MID     field type      offset in payload
#define FIELDS(X) \
    X(nav_valid,        41,     T_2D,           0)  \
    X(nav_type,         41,     T_2D,           2)  \
    X(year,             41,     T_2U,           8)  \
    X(month,            41,     T_1U,           10) \
    X(day,              41,     T_1U,           11) \
    X(hour,             41,     T_1U,           12) \
    X(minute,           41,     T_1U,           13) \
    X(second,           41,     T_2U,           14) \
    X(satellites,       41,     T_4D,           16) \
    X(latitude,         41,     T_4S,           20) \
    X(longitude,        41,     T_4S,           24) \
    X(altitude_ellip,   41,     T_4S,           28) \
    X(hdop,             41,     T_1U,           89)

struct field {
    uint16_t    struct_offset, payload_offset;
    uint8_t     mid, size;
};

#define FIELD(name, mid, type, offset) \
    { (uint16_t)offsetof(struct gps_data, name), offset, mid, type },

static const struct field fields[] = {
    FIELDS(FIELD)
};

static void
update_gps_data(struct gps_data *data, uint8_t *msg)
{
    uint8_t val1;
    uint16_t val2;
    uint32_t val4;
    uint64_t val8 = 0;

    uint8_t mid = msg[0];
    for (unsigned i = 0; i < ARRAY_LEN(fields); i++) {
        if (fields[i].mid != mid) {
            continue;
        }
        const struct field *f = &(fields[i]);
        uint8_t *b = msg + f->payload_offset + 1;
        unsigned nbytes = f->size;
        uint8_t *dest = (uint8_t *)data + f->struct_offset;
        for (unsigned n = 0; n < nbytes; n++) {
            val8 = (val8 << 8) | b[n];
        }
        switch (nbytes) {
        case 1:
            val1 = (uint8_t)val8;
            memcpy(dest, &val1, 1);
            break;
        case 2:
            val2 = (uint16_t)val8;
            memcpy(dest, &val2, 2);
            break;
        case 4:
            val4 = (uint32_t)val8;
            memcpy(dest, &val4, 4);
            break;
        case 8:
            memcpy(dest, &val8, 8);
            break;
        }
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
    struct gps_data data;
    update_gps_data(&data, mid41);

#define SHOW(name, mid, type, offset) printf(#name " = %08X\n", data.name);
    FIELDS(SHOW);

    return 0;
}
