#pragma once
#include "undocumented.h"
#include <intrin.h>



PEPROCESS process;
VOID ReadMemory(ULONG64 address, PVOID buffer, SIZE_T size)
{
	KAPC_STATE  apc_state;
	KeStackAttachProcess(process, &apc_state);

	if (MmIsAddressValid((PVOID)address) && MmIsAddressValid((PVOID)(address + size)))
	{
		RtlCopyMemory(buffer, (PVOID)address, size);
	}
	else
	{

	}
	KeUnstackDetachProcess(&apc_state);
}


VOID WriteMemory(ULONG64 address, PVOID buffer, SIZE_T size)
{
	KAPC_STATE  apc_state;
	KeStackAttachProcess(process, &apc_state);
	if (MmIsAddressValid((PVOID)address) && MmIsAddressValid((PVOID)(address + size)))
	{
		KIRQL   tempirql = KeRaiseIrqlToDpcLevel();

		ULONG64  cr0 = __readcr0();

		cr0 &= 0xfffffffffffeffff;
		__writecr0(cr0);

		_disable();

		RtlCopyMemory((PVOID)address, buffer, size);

		cr0 = __readcr0();

		cr0 |= 0x10000;

		_enable();

		__writecr0(cr0);

		KeLowerIrql(tempirql);
	}
	else
	{

	}
	KeUnstackDetachProcess(&apc_state);
}



PVOID GetUserModule(IN PEPROCESS pProcess, IN PUNICODE_STRING ModuleName, IN BOOLEAN isWow64)
{
	ASSERT(pProcess != NULL);
	if (pProcess == NULL)
		return NULL;

	// Protect from UserMode AV
	__try
	{
		LARGE_INTEGER time = { 0 };
		time.QuadPart = -250ll * 10 * 1000;     // 250 msec.

		// Wow64 process
		if (isWow64)
		{
			PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(pProcess);
			if (pPeb32 == NULL)
			{
				return NULL;
			}

			// Wait for loader a bit
			for (INT i = 0; !pPeb32->Ldr && i < 10; i++)
			{
				KeDelayExecutionThread(KernelMode, TRUE, &time);
			}

			// Still no loader
			if (!pPeb32->Ldr)
			{
				return NULL;
			}

			// Search in InLoadOrderModuleList
			for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList.Flink;
				pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList;
				pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
			{
				UNICODE_STRING ustr;
				PLDR_DATA_TABLE_ENTRY32 pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

				RtlUnicodeStringInit(&ustr, (PWCH)pEntry->BaseDllName.Buffer);

				if (RtlCompareUnicodeString(&ustr, ModuleName, TRUE) == 0)
					return (PVOID)pEntry->DllBase;
			}
		}
		// Native process
		else
		{
			PPEB pPeb = PsGetProcessPeb(pProcess);
			if (!pPeb)
			{
				return NULL;
			}

			
			for (INT i = 0; !pPeb->Ldr && i < 10; i++)
			{
				KeDelayExecutionThread(KernelMode, TRUE, &time);
			}

			if (!pPeb->Ldr)
			{
				return NULL;
			}

			// Search in InLoadOrderModuleList
			for (PLIST_ENTRY pListEntry = pPeb->Ldr->InLoadOrderModuleList.Flink;
				pListEntry != &pPeb->Ldr->InLoadOrderModuleList;
				pListEntry = pListEntry->Flink)
			{
				PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
				if (RtlCompareUnicodeString(&pEntry->BaseDllName, ModuleName, TRUE) == 0)
					return pEntry->DllBase;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return NULL;
}


template <class T> T	READ(ULONG64 address)
{

	T buffer{};
	ReadMemory(address, &buffer, sizeof(T));
	return buffer;
}
template <class T> void  Write(ULONG64 address, T buffer)
{
	WriteMemory(address, &buffer, sizeof(T));
}