#include <efi.h>
#include <efilib.h>
#include "memory_map.h"
#include "elf.h"

#define MAX(A, B) (A > B ? A : B)
#define MIN(A, B) (A < B ? A : B)

EFI_STATUS GetMemoryMap(struct MemoryMap *map){
    if(map->buffer == NULL){
        return EFI_BUFFER_TOO_SMALL;
    }
    map->map_size = map->buffer_size;
    return uefi_call_wrapper(gBS->GetMemoryMap, 5,
                             &map->map_size,
                             (EFI_MEMORY_DESCRIPTOR *) map->buffer,
                             &map->map_key,
                             &map->descriptor_size,
                             &map->descriptor_version);
}

const CHAR16 *GetMemoryTypeUnicode(EFI_MEMORY_TYPE type){
    switch(type){
        case EfiReservedMemoryType:
            return L"EfiReservedMemoryType";
        case EfiLoaderCode:
            return L"EfiLoaderCode";
        case EfiLoaderData:
            return L"EfiLoaderData";
        case EfiBootServicesCode:
            return L"EfiBootServicesCode";
        case EfiBootServicesData:
            return L"EfiBootServicesData";
        case EfiRuntimeServicesCode:
            return L"EfiRuntimeServicesCode";
        case EfiRuntimeServicesData:
            return L"EfiRuntimeServicesData";
        case EfiConventionalMemory:
            return L"EfiConventionalMemory";
        case EfiUnusableMemory:
            return L"EfiUnusableMemory";
        case EfiACPIReclaimMemory:
            return L"EfiACPIReclaimMemory";
        case EfiACPIMemoryNVS:
            return L"EfiACPIMemoryNVS";
        case EfiMemoryMappedIO:
            return L"EfiMemoryMappedIO";
        case EfiMemoryMappedIOPortSpace:
            return L"EfiMemoryMappedIOPortSpace";
        case EfiPalCode:
            return L"EfiPalCode";
        // case EfiPersistentMemory:
        //     return L"EfiPersistentMemory";
        case EfiMaxMemoryType:
            return L"EfiMaxMemoryType";
        default:
            return L"InvalidMemoryType";
    }
}

VOID PrintMemoryMap(struct MemoryMap *map){
    Print(L"|Index|Type|         TypeName         |PhysicalStart|NumberOfPages|Attribute|\n");
    EFI_PHYSICAL_ADDRESS iter;
    int i;
    for(iter = (EFI_PHYSICAL_ADDRESS) map->buffer, i = 0;
        iter < (EFI_PHYSICAL_ADDRESS) map->buffer + map->map_size;
        iter += map->descriptor_size, i++){
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *) iter;
        Print(L"| %3u | %2x | %24-ls | %11lx | %11lx | %7lx |\n",
              i, desc->Type, GetMemoryTypeUnicode(desc->Type),
              desc->PhysicalStart, desc->NumberOfPages,
              desc->Attribute & 0xffffflu);
    }
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL **root){
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;

    status = uefi_call_wrapper(gBS->OpenProtocol, 6,
                               image_handle,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID * *) & loaded_image,
                               image_handle,
                               NULL,
                               EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if(EFI_ERROR(status)){
        return status;
    }

    status = uefi_call_wrapper(gBS->OpenProtocol, 6,
                               loaded_image->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID * *) & fs,
                               image_handle,
                               NULL,
                               EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if(EFI_ERROR(status)){
        return status;
    }
    return uefi_call_wrapper(fs->OpenVolume, 2, fs, root);
}

void CalcLoadAddressRange(Elf64_Ehdr *ehdr, UINT64 *first, UINT64 *last){
    Elf64_Phdr *phdr = (Elf64_Phdr *) ((UINT64) ehdr + ehdr->e_phoff);
    // *first = MAX_UINT64;
    *first = 0xffffffffffffffff;
    *last = 0;
    for(Elf64_Half i = 0; i < ehdr->e_phnum; ++i){
        if(phdr[i].p_type != PT_LOAD) continue;
        *first = MIN(*first, phdr[i].p_vaddr);
        *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
    }
}

void CopyLoadSegments(Elf64_Ehdr *ehdr){
    Elf64_Phdr *phdr = (Elf64_Phdr *) ((UINT64) ehdr + ehdr->e_phoff);
    for(Elf64_Half i = 0; i < ehdr->e_phnum; ++i){
        if(phdr[i].p_type != PT_LOAD) continue;

        UINT64 segm_in_file = (UINT64) ehdr + phdr[i].p_offset;
        CopyMem((VOID *) phdr[i].p_vaddr, (VOID *) segm_in_file, phdr[i].p_filesz);

        UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        SetMem((VOID *) (phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
    }
}

void Halt(void){
    while(1) __asm__("hlt");
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table){
    InitializeLib(image_handle, system_table);

    uefi_call_wrapper(system_table->BootServices->SetWatchdogTimer, 4, 0, 0, 0, NULL);

    EFI_STATUS status;

    Print(L"Hello bootloader!\n");

    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    status = GetMemoryMap(&memmap);
    if(EFI_ERROR(status)){
        Print(L"Failed to get memory map: %r\n", status);
        Halt();
    }
    Print(L"Succeed to get memory map (map->buffer = %08lx, map->map_size = %08lx)\n", memmap.buffer, memmap.map_size);
    PrintMemoryMap(&memmap);

    EFI_FILE_PROTOCOL *root_dir;
    status = OpenRootDir(image_handle, &root_dir);
    if(EFI_ERROR(status)){
        Print(L"Failed to open root directory: %r\n", status);
        Halt();
    }

    EFI_FILE_PROTOCOL *kernel_file;
    status = uefi_call_wrapper(root_dir->Open, 5, root_dir, &kernel_file, L"\\kernel.elf",
                               EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(status)){
        Print(L"Failed to open file '\\kernel.elf': %r\n", status);
        Halt();
    }

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    UINT8 file_info_buffer[file_info_size];
    status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &gEfiFileInfoGuid,
                               &file_info_size, file_info_buffer);
    if(EFI_ERROR(status)){
        Print(L"Failed to get file information: %r\n", status);
        Halt();
    }

    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *) file_info_buffer;
    UINTN kernel_file_size = file_info->FileSize;

    VOID *kernel_buffer;
    status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, kernel_file_size, &kernel_buffer);
    if(EFI_ERROR(status)){
        Print(L"Failed to allocate pool: %r\n", status);
        Halt();
    }
    status = uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &kernel_file_size, kernel_buffer);
    if(EFI_ERROR(status)){
        Print(L"Error: %r", status);
        Halt();
    }
    Elf64_Ehdr *kernel_ehdr = (Elf64_Ehdr *) kernel_buffer;
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

    UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
    status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAddress, EfiLoaderData,
                               num_pages, &kernel_first_addr);
    if(EFI_ERROR(status)){
        Print(L"Failed to allocate pages: %r\n", status);
        Halt();
    }

    CopyLoadSegments(kernel_ehdr);
    Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    status = uefi_call_wrapper(gBS->FreePool, 1, kernel_buffer);
    if(EFI_ERROR(status)){
        Print(L"Failed to free pool: %r\n", status);
        Halt();
    }

    VOID *acpi_table = NULL;
    EFI_GUID acpi_table_guid = ACPI_20_TABLE_GUID;
    for(UINTN i = 0; i < system_table->NumberOfTableEntries; ++i){
        if(CompareGuid(&system_table->ConfigurationTable[i].VendorGuid, &acpi_table_guid) == 0){
            acpi_table = system_table->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    status = uefi_call_wrapper(gBS->ExitBootServices, 2, image_handle, memmap.map_key);
    if(EFI_ERROR(status)){
        status = GetMemoryMap(&memmap);
        if(EFI_ERROR(status)){
            Print(L"Failed to get memory map: %r\n", status);
            Halt();
        }
        status = uefi_call_wrapper(gBS->ExitBootServices, 2, image_handle, memmap.map_key);
        if(EFI_ERROR(status)){
            Print(L"Could not exit boot service: %r\n", status);
            Halt();
        }
    }

    UINT64 entry_addr = *(UINT64 *) (kernel_first_addr + 24);

    typedef void EntryPointType(const struct MemoryMap *, const VOID *);
    EntryPointType *entry_point = (EntryPointType *) entry_addr;
    entry_point(&memmap, acpi_table);

    Halt();

    return EFI_SUCCESS;
}