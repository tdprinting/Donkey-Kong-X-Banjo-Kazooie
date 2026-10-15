/*
 * C API for the Jiggy Bridge.
 *
 * This is the surface the DK64 Recompiled host wraps as recomp imports, and the one an
 * Android front end calls after the player picks their ROMs (open_memory takes bytes
 * read from a content URI, since Android apps can't rely on plain file paths).
 *
 * Read functions follow one convention: they return the full size of the data in bytes,
 * or -1 on error. Passing dst = NULL (or a buffer that is too small) copies nothing, so
 * callers can query the size first.
 */
#ifndef JIGGY_BRIDGE_H
#define JIGGY_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JiggyBridge JiggyBridge;

/* Opens both ROMs from disk. Returns NULL and fills `error` on failure. */
JiggyBridge* jiggy_bridge_open(const char* banjo_rom_path, const char* dk64_rom_path, char* error, size_t error_len);

/* Opens both ROMs from memory. The bytes are copied. */
JiggyBridge* jiggy_bridge_open_memory(const void* banjo_rom, size_t banjo_len, const void* dk64_rom, size_t dk64_len,
                                      char* error, size_t error_len);

void jiggy_bridge_close(JiggyBridge* bridge);

uint32_t jiggy_bk_asset_count(const JiggyBridge* bridge);
int64_t jiggy_bk_asset_read(const JiggyBridge* bridge, uint32_t asset_id, void* dst, size_t dst_len);

uint32_t jiggy_dk64_file_count(const JiggyBridge* bridge, uint32_t table);
int64_t jiggy_dk64_file_read(const JiggyBridge* bridge, uint32_t table, uint32_t index, void* dst, size_t dst_len);

#ifdef __cplusplus
}
#endif

#endif
