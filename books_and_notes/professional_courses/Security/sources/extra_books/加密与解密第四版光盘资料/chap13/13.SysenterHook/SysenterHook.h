/*

  SysenterHook.H

  Author: <your name>
  Last Updated: 2006-02-12

  This framework is generated by EasySYS 0.3.0
  This template file is copying from QuickSYS 0.3.0 written by Chunhua Liu

*/

#ifndef _SYSENTERHOOK_H
#define _SYSENTERHOOK_H 1

//
// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-0x7FFF(32767), and 0x8000(32768)-0xFFFF(65535)
// are reserved for use by customers.
//

#define FILE_DEVICE_SYSENTERHOOK	0x8000

//
// Macro definition for defining IOCTL and FSCTL function control codes. Note
// that function codes 0-0x7FF(2047) are reserved for Microsoft Corporation,
// and 0x800(2048)-0xFFF(4095) are reserved for customers.
//

#define SYSENTERHOOK_IOCTL_BASE	0x800

//
// The device driver IOCTLs
//

#define CTL_CODE_SYSENTERHOOK(i) CTL_CODE(FILE_DEVICE_SYSENTERHOOK, SYSENTERHOOK_IOCTL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SYSENTERHOOK_HELLO	CTL_CODE_SYSENTERHOOK(0)
#define IOCTL_SYSENTERHOOK_TEST	CTL_CODE_SYSENTERHOOK(1)

//
// Name that Win32 front end will use to open the SysenterHook device
//

#define SYSENTERHOOK_WIN32_DEVICE_NAME_A	"\\\\.\\SysenterHook"
#define SYSENTERHOOK_WIN32_DEVICE_NAME_W	L"\\\\.\\SysenterHook"
#define SYSENTERHOOK_DEVICE_NAME_A			"\\Device\\SysenterHook"
#define SYSENTERHOOK_DEVICE_NAME_W			L"\\Device\\SysenterHook"
#define SYSENTERHOOK_DOS_DEVICE_NAME_A		"\\DosDevices\\SysenterHook"
#define SYSENTERHOOK_DOS_DEVICE_NAME_W		L"\\DosDevices\\SysenterHook"

#ifdef _UNICODE
#define SYSENTERHOOK_WIN32_DEVICE_NAME SYSENTERHOOK_WIN32_DEVICE_NAME_W
#define SYSENTERHOOK_DEVICE_NAME		SYSENTERHOOK_DEVICE_NAME_W
#define SYSENTERHOOK_DOS_DEVICE_NAME	SYSENTERHOOK_DOS_DEVICE_NAME_W
#else
#define SYSENTERHOOK_WIN32_DEVICE_NAME SYSENTERHOOK_WIN32_DEVICE_NAME_A
#define SYSENTERHOOK_DEVICE_NAME		SYSENTERHOOK_DEVICE_NAME_A
#define SYSENTERHOOK_DOS_DEVICE_NAME	SYSENTERHOOK_DOS_DEVICE_NAME_A
#endif

#endif