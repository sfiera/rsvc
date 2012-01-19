#!/usr/sbin/dtrace -s
#pragma D option quiet

typedef struct {
    uint64_t    offset;
    uint8_t     sectorArea;
    uint8_t     sectorType;
    uint8_t     reserved0080[6];
    uint32_t    bufferLength;
    uint32_t    buffer;
} dk_cd_read32_t;
dk_cd_read32_t *cd_read32;

typedef struct {
    uint64_t    offset;
    uint8_t     sectorArea;
    uint8_t     sectorType;
    uint8_t     reserved0080[10];
    uint32_t    bufferLength;
    uint64_t    buffer;
} dk_cd_read64_t;
dk_cd_read64_t *cd_read64;

BEGIN {
    DKIOCCDREAD32               = 0xC0186460;
    DKIOCCDREAD64               = 0xC0206460;
    DKIOCCDREADISRC             = 0xC0106461;
    DKIOCCDREADMCN              = 0xC0106462;
    DKIOCCDREADTOC32            = 0xC0106464;
    DKIOCCDREADTOC64            = 0xC0186464;

    commands[DKIOCCDREAD32]     = "DKIOCCDREAD32";
    commands[DKIOCCDREAD64]     = "DKIOCCDREAD64";
    commands[DKIOCCDREADISRC]   = "DKIOCCDREADISRC";
    commands[DKIOCCDREADMCN]    = "DKIOCCDREADMCN";
    commands[DKIOCCDREADTOC32]  = "DKIOCCDREADTOC32";
    commands[DKIOCCDREADTOC64]  = "DKIOCCDREADTOC64";
}

syscall::ioctl:entry
/(commands[arg1] != "")/
{
    self->command = commands[arg1];
    printf("E %5d %3d %-16s\n", pid, arg0, commands[arg1]);
}

syscall::ioctl:entry
/(arg1 == DKIOCCDREAD32)/
{
    cd_read32 = copyin(arg2, sizeof(*cd_read32));
    printf("            offset: %u\n", cd_read32->offset);
    printf("            sectorArea: %x\n", cd_read32->sectorArea);
    printf("            sectorType: %x\n", cd_read32->sectorType);
    printf("            bufferLength: %u\n", cd_read32->bufferLength);
    printf("            buffer: 0x%08x\n", cd_read32->buffer);
}

syscall::ioctl:entry
/(arg1 == DKIOCCDREAD64)/
{
    cd_read64 = copyin(arg2, sizeof(*cd_read64));
    printf("            offset: %u\n", cd_read64->offset);
    printf("            sectorArea: %x\n", cd_read64->sectorArea);
    printf("            sectorType: %x\n", cd_read64->sectorType);
    printf("            bufferLength: %u\n", cd_read64->bufferLength);
    printf("            buffer: 0x%016x\n", cd_read64->buffer);
}

syscall::ioctl:return
/self->command != ""/
{
    printf("R %5d %3d %-16s %5d\n", pid, arg0, self->command, errno);
    self->command = "";
}
