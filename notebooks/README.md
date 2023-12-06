les étapes du code : 

get_boot_attrs : 

# demander VERSION
    read_version_response = _send_and_receive(
        connection,
        Command(CommandCode.READ_VERSION),
    )

# demander MEMORY_RANGE
    mem_range_response = _send_and_receive(
        connection,
        Command(CommandCode.GET_MEMORY_ADDRESS_RANGE),
    )

    assert isinstance(mem_range_response, MemoryRange)

    # program_end + 2 explanation:
    # +1 because the upper bound reported by the bootloader is inclusive, but we
    # want to use it as a Python range, which is half-open.
    # +1 because the final byte of the final 24-bit instruction is not included in
    # the range reported by the bootloader, but it is still writable.
    return mem_range_response.program_start, mem_range_response.program_end + 2


# demander CHECKSUM (peut planter)
    try:
        _get_remote_checksum(connection, memory_range[0], write_size)
        has_checksum = True
    except UnsupportedCommand:
        _logger.warning("Bootloader does not support checksumming")
        has_checksum = False

    checksum_response = _send_and_receive(
        connection,
        Command(
            command=CommandCode.CALC_CHECKSUM,
            data_length=length,
            address=address,
        ),
    )
    assert isinstance(checksum_response, Checksum)

# on a tous les boot_attrs : 
    return BootAttrs(
        version,
        max_packet_length,
        device_id,
        erase_size,
        write_size,
        memory_range,
        has_checksum,
    )


# ensuite, on chunke tout : 

total_bytes, chunks = bf.chunked(hexfile=<HEXFILE_PATH_STRING>, bootattrs)


# on erase la flash : 
    _send_and_receive(
        connection,
        command=Command(
            command=CommandCode.ERASE_FLASH,
            data_length=(end - start) // erase_size,
            unlock_sequence=_FLASH_UNLOCK_KEY,
            address=start,
        ),
    )


# pour tous les chunks, on fait : 

    _send_and_receive(
        connection,
        Command(
            command=CommandCode.WRITE_FLASH,
            data_length=len(chunk.data),
            unlock_sequence=_FLASH_UNLOCK_KEY,
            address=chunk.address,
        ),
        chunk.data,
    )




le programme python total :


# Connect to a device in bootloader mode.
connection = serial.Serial(port=<PORT>, baudrate=<BAUDRATE>, timeout=<TIMEOUT>)
# Query its attributes.
bootattrs = bf.get_boot_attrs(connection)
# Load the firmware image and split it into chunks.
total_bytes, chunks = bf.chunked(hexfile=<HEXFILE_PATH_STRING>, bootattrs)
# Erase the device's program memory area.
bf.erase_flash(connection, bootattrs.memory_range, bootattrs.erase_size)

# Write the firmware chunks to the bootloader in a loop.
for chunk in chunks:
    bf.write_flash(connection, chunk)

    # Optionally, check that the write is OK by checksumming.
    if bootattrs.has_checksum:
        bf.checksum(connection, chunk)

    # At this point, you may want to give an indication of the flashing progress,
    # like updating a progress bar.

# Verify that the new application is detected.
bf.self_verify(connection)

