"""Utility functions."""
from __future__ import annotations

from typing import Iterator

import importlib
import _bincopy
import _bincopy as bincopy # type: ignore[import-not-found]
importlib.reload(_bincopy)

from _types import BootAttrs, Chunk, Command


def chunked(
    hexfile: str,
    bootattrs: BootAttrs,
) -> tuple[int, Iterator[Chunk]]:
    """Split a HEX file into chunks.

    Parametersé
    ----------
    hexfile : str
        Path of a HEX file containing application firmare.
    bootattrs : BootAttrs
        The bootloader's attributes, as read by `get_boot_attrs`.

    Returns
    -------
    total_bytes : int
        The total number of bytes in all chunks.
    chunks : Iterator[Chunk]
        Appropriatelly sized chunks of data, suitable for writing in a loop with
        `write_flash`.

    Raises
    ------
    ValueError
        If HEX file contains no data in program memory range.
    """
    hexdata = bincopy.BinFile()
    with open(hexfile, 'r') as fin:
        hexdata.add_microchip_hex(fin.read(), False)

    debug_parsed_records = hexdata.debug_parsed_records
    
    print("number of debug_parsed_records", len(debug_parsed_records))

    print("first debug_parsed_records ", debug_parsed_records[0])
    print("second debug_parsed_records ", debug_parsed_records[1])
    print("n-th debug_parsed_records ", debug_parsed_records[127])

    # print("42th segment ", hexdata._segments[42])

    debug_segments_before_crop = [bincopy.Segment(s.minimum_address, s.maximum_address, s.data.copy(), s.word_size_bytes) for s in hexdata.segments._list]

    print("debug_segments_before_crop len : ", len(debug_segments_before_crop))

    hexdata.crop(*bootattrs.memory_range)


    chunk_size = bootattrs.max_packet_length - Command.get_size()
    chunk_size -= chunk_size % bootattrs.write_size
    chunk_size //= hexdata.word_size_bytes
    total_bytes = len(hexdata) * hexdata.word_size_bytes

    if total_bytes == 0:
        msg = "HEX file contains no data within program memory range"
        raise ValueError(msg)

    total_bytes += (bootattrs.write_size - total_bytes) % bootattrs.write_size
    align = bootattrs.write_size // hexdata.word_size_bytes
    return total_bytes, hexdata.segments.chunks(chunk_size, align, b"\x00\x00"), debug_parsed_records, debug_segments_before_crop, hexdata.segments
